#include "fast_compares.c"

static int (*compare_function)(PyObject* v, PyObject* w, int op);

assign_compare_function(PyObject* first_item,
                        int keys_are_all_same_type,
                        int strings_are_latin,
                        int ints_are_bounded,
                        int tuples_are_homogenous)
{
  if (keys_are_all_same_type) {
    PyTypeObject* key_type = first_item->ob_type;
    if (key_type == &PyUnicode_Type) {
      compare_function = (strings_are_latin ? \
                          unsafe_latin_unicode_compare :
                          unsafe_general_unicode_compare);
    }
    else if (key_type == &PyLong_Type) {
      compare_function = (ints_are_bounded ? \
                          unsafe_small_long_compare :
                          unsafe_general_long_compare);
    }
    else if (key_type == &PyFloat_Type) {
      compare_function = unsafe_float_compare;
    }
    else if (key_type == &PyTuple_Type) {
      /* Now let's see if we can optimize the elementwise compares between tuples:
       * are the tuples homogenous with respect to type?
       */
      if (tuples_are_homogenous) {
        compare_function = unsafe_homogenous_tuple_compare;
        int i;
        for (i=0; i<first_item->ob_size; i++) {
          PyTypeObject* element_type = PyTuple_GET_ITEM(first_item,i)->ob_type;
          if (element_type == &PyUnicode_Type)
            elem_compare_funcs[i] = unsafe_general_unicode_compare;
          else if (element_type = &PyLong_Type)
            elem_compare_funcs[i] = unsafe_general_long_compare;
          else if (element_type = &PyFloat_Type)
            elem_compare_funcs[i] = unsafe_general_float_compare;
          else
            elem_compare_funcs[i] = PyObject_RichCompareBool;
        }
      } else {
        compare_function = unsafe_heterogenous_tuple_compare;
      }
    }
    else {
      compare_function = tp_richcompare_wrapper;
      tp_richcompare = first_item->op_type->tp_richcompare;
    }
  } else {
    compare_function = PyObject_RichCompareBool;
  }
}

#define ISLT(X, Y) ((*compare_function)(X, Y, Py_LT))
#define IFLT(X, Y) if ((k = ISLT(X, Y)) < 0) goto fail; \
  if (k)

#include "listsort_helpers.c"

static PyObject *
fast_listsort(FastListObject *self_fastlist, PyObject *args, PyObject *kwds)
{
    PyListObject* self = (PyListObject*)self_fastlist;
    MergeState ms;
    Py_ssize_t nremaining;
    Py_ssize_t minrun;
    sortslice lo;
    Py_ssize_t saved_ob_size, saved_allocated;
    PyObject **saved_ob_item;
    PyObject **final_ob_item;
    PyObject *result = NULL;            /* guilty until proved innocent */
    int reverse = 0;
    PyObject *keyfunc = NULL;
    Py_ssize_t i;
    static char *kwlist[] = {"key", "reverse", 0};
    PyObject **keys;

    assert(self != NULL);
    assert (PyList_Check(self));
    if (args != NULL) {
        if (!PyArg_ParseTupleAndKeywords(args, kwds, "|Oi:sort",
            kwlist, &keyfunc, &reverse))
            return NULL;
        if (Py_SIZE(args) > 0) {
            PyErr_SetString(PyExc_TypeError,
                "must use keyword argument for key function");
            return NULL;
        }
    }
    if (keyfunc == Py_None)
        keyfunc = NULL;

    /* The list is temporarily made empty, so that mutations performed
     * by comparison functions can't affect the slice of memory we're
     * sorting (allowing mutations during sorting is a core-dump
     * factory, since ob_item may change).
     */
    saved_ob_size = Py_SIZE(self);
    saved_ob_item = self->ob_item;
    saved_allocated = self->allocated;
    Py_SIZE(self) = 0;
    self->ob_item = NULL;
    self->allocated = -1; /* any operation will reset it to >= 0 */

    if (keyfunc == NULL) {
        keys = NULL;
        lo.keys = saved_ob_item;
        lo.values = NULL;
    }
    else {
        if (saved_ob_size < MERGESTATE_TEMP_SIZE/2)
            /* Leverage stack space we allocated but won't otherwise use */
            keys = &ms.temparray[saved_ob_size+1];
        else {
            keys = PyMem_MALLOC(sizeof(PyObject *) * saved_ob_size);
            if (keys == NULL) {
                PyErr_NoMemory();
                goto keyfunc_fail;
            }
        }

        //Get type of the first key
        keys[0] = PyObject_CallFunctionObjArgs(keyfunc, saved_ob_item[0], NULL);

        for (i = 0; i < saved_ob_size ; i++) {
            keys[i] = PyObject_CallFunctionObjArgs(keyfunc, saved_ob_item[i],
                                                   NULL);

            if (keys[i] == NULL) {
                for (i=i-1 ; i>=0 ; i--)
                    Py_DECREF(keys[i]);
                if (saved_ob_size >= MERGESTATE_TEMP_SIZE/2)
                    PyMem_FREE(keys);
                goto keyfunc_fail;
            }
        }

        lo.keys = keys;
        lo.values = saved_ob_item;
    }

    /* Turn off type checking if all keys are same type,
     * by replacing PyObject_RichCompare with lo.keys[0]->ob_type->tp_richcompare,
     * and possibly also use optimized comparison functions if keys are strings or ints.
     */

    /* Get information about the first element of the list */
    int keys_are_all_same_type = 1;
    PyTypeObject* key_type = lo.keys[0]->ob_type;
    int strings_are_latin = 1; int ints_are_bounded = 1; int tuples_are_homogenous = 1;
    int tuple_size; PyTypeObject** tuple_key_types;
    if (key_type == &PyTuple_Types){
      tuple_size = Py_SIZE(lo.keys[0]);
      tuple_key_types = (PyTypeObject**)malloc(saved_ob_size*sizeof(PyTypeObject*));
      for (int j=0; j<tuple_size; j++)
        tuple_key_types[i] = PyTuple_GET_ITEM(lo.keys[0],j)->ob_type;
    }
    
    /* Verify the info about the first list element holds for the entire list */
    for (i=0; i< saved_ob_size; i++) {
      if (lo.keys[i]->ob_type != key_type)
        {
          keys_are_all_same_type = 0;
          break;
        }
      else if (key_type == &PyLong_Type && strings_are_latin)
        {
          if (Py_SIZE(lo.keys[i]) < -1 || Py_SIZE(lo.keys[i]) > 1)
            ints_are_bounded = 0;
        }
      else if (key_type == &PyUnicode_Type && ints_are_bounded)
        {
          if (PyUnicode_KIND(lo.keys[i]) != PyUnicode_1BYTE_KIND)
            strings_are_latin = 0;
        }
      else if (key_type == &PyTuple_Type && tuples_are_homogenous)
        {
          if (Py_SIZE(lo.keys[i]) != tuple_size){
            tuples_are_homogenous = 0;
          } else {
            for (int j=0; j<tuple_size; j++){
              if (PyTuple_GET_ITEM(lo.keys[i],j)->ob_type != tuple_key_types[j])
                tuples_are_homogenous = 0;
            }
          }
        }
    }
    free(tuple_key_types);

    assign_compare_function(lo.keys[0],
                            keys_are_all_same_type,
                            strings_are_latin,
                            ints_are_bounded,
                            tuples_are_homogenous);
    /* End of type-checking stuff! */

    merge_init(&ms, saved_ob_size, keys != NULL);

    nremaining = saved_ob_size;
    if (nremaining < 2)
        goto succeed;

    /* Reverse sort stability achieved by initially reversing the list,
    applying a stable forward sort, then reversing the final result. */
    if (reverse) {
        if (keys != NULL)
            reverse_slice(&keys[0], &keys[saved_ob_size]);
        reverse_slice(&saved_ob_item[0], &saved_ob_item[saved_ob_size]);
    }

    /* March over the array once, left to right, finding natural runs,
     * and extending short natural runs to minrun elements.
     */
    minrun = merge_compute_minrun(nremaining);
    do {
        int descending;
        Py_ssize_t n;

        /* Identify next run. */
        n = count_run(lo.keys, lo.keys + nremaining, &descending);
        if (n < 0)
            goto fail;
        if (descending)
            reverse_sortslice(&lo, n);
        /* If short, extend to min(minrun, nremaining). */
        if (n < minrun) {
            const Py_ssize_t force = nremaining <= minrun ?
                              nremaining : minrun;
            if (binarysort(lo, lo.keys + force, lo.keys + n) < 0)
                goto fail;
            n = force;
        }
        /* Push run onto pending-runs stack, and maybe merge. */
        assert(ms.n < MAX_MERGE_PENDING);
        ms.pending[ms.n].base = lo;
        ms.pending[ms.n].len = n;
        ++ms.n;
        if (merge_collapse(&ms) < 0)
            goto fail;
        /* Advance to find next run. */
        sortslice_advance(&lo, n);
        nremaining -= n;
    } while (nremaining);

    if (merge_force_collapse(&ms) < 0)
        goto fail;
    assert(ms.n == 1);
    assert(keys == NULL
           ? ms.pending[0].base.keys == saved_ob_item
           : ms.pending[0].base.keys == &keys[0]);
    assert(ms.pending[0].len == saved_ob_size);
    lo = ms.pending[0].base;

succeed:
    result = Py_None;
fail:
    if (keys != NULL) {
        for (i = 0; i < saved_ob_size; i++)
            Py_DECREF(keys[i]);
        if (saved_ob_size >= MERGESTATE_TEMP_SIZE/2)
            PyMem_FREE(keys);
    }

    if (self->allocated != -1 && result != NULL) {
        /* The user mucked with the list during the sort,
         * and we don't already have another error to report.
         */
        PyErr_SetString(PyExc_ValueError, "list modified during sort");
        result = NULL;
    }

    if (reverse && saved_ob_size > 1)
        reverse_slice(saved_ob_item, saved_ob_item + saved_ob_size);

    merge_freemem(&ms);

keyfunc_fail:
    final_ob_item = self->ob_item;
    i = Py_SIZE(self);
    Py_SIZE(self) = saved_ob_size;
    self->ob_item = saved_ob_item;
    self->allocated = saved_allocated;
    if (final_ob_item != NULL) {
        /* we cannot use list_clear() for this because it does not
           guarantee that the list is really empty when it returns */
        while (--i >= 0) {
            Py_XDECREF(final_ob_item[i]);
        }
        PyMem_FREE(final_ob_item);
    }
    Py_XINCREF(result);
    return result;
}
#undef IFLT
#undef ISLT

