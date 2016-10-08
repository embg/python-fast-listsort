/* These functions return Py_True if x<y, Py_False if x >= y, and NULL if error,
 * just like the RichCompare functions. But they're faster for these special cases.
 */
#include "fast_compares.c"
PyObject* unsafe_unicode_compare(PyObject* left, PyObject* right, int not_used){
  /* Reference implementation: PyUnicode_Compare in Objects/unicodeobject.c
   * left and right are assumed to both be PyUnicode_Type
   */
  if (PyUnicode_READY(left) == -1 ||
      PyUnicode_READY(right) == -1)
    return NULL;
  return unicode_compare(left, right) == -1 ? Py_True : Py_False;
}

PyObject* unsafe_long_compare(PyObject* left, PyObject* right, int not_used){
  /* Reference implementation: long_richcompare in Objects/longobject.c
   * left and right are assumed to both be PyLong_Type
   */
  return long_compare((PyLongObject*)left, (PyLongObject*)right) == -1 ? Py_True : Py_False;
}

PyObject* (*compare_function)(PyObject* left, PyObject* right, int op) = PyObject_RichCompare;
PyObject* cmp_result;
#define ISLT(X, Y) ((*compare_function)(X,Y,Py_LT))
#define IFLT(X, Y) if ((cmp_result = ISLT(X, Y)) == NULL) goto fail; \
  if (cmp_result == Py_True)

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
    int keys_are_all_same_type = 1;
    PyTypeObject* key_type = lo.keys[0]->ob_type;
    for (i=0; i< saved_ob_size; i++)
      if (lo.keys[i]->ob_type != key_type)
        keys_are_all_same_type = 0;

    if (keys_are_all_same_type){
      if (key_type == &PyUnicode_Type)
        compare_function = unsafe_unicode_compare;
      if (key_type == &PyLong_Type)
        compare_function = unsafe_long_compare;
      else
        compare_function = key_type->tp_richcompare;
    }
    /* End of evil type checking stuff */

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
