#define Py_DEBUG 0

//Function pointers set at runtime if necessary
PyObject* (*richcompare_function)(PyObject* v, PyObject* w, int op);
 int (*tuple_elem_compare)(PyObject* v, PyObject* w);

//Generic safe comparison of any two objects of the same type
 int safe_object_compare(PyObject* v, PyObject* w){
  return PyObject_RichCompareBool(v, w, Py_LT);
}

int unsafe_object_compare(PyObject* v, PyObject* w){
#ifdef Py_DEBUG
  assert(v->ob_type->tp_richcompare == richcompare_function &&
         w->ob_type->tp_richcompare == richcompare_function);
#endif

  PyObject* res = (*richcompare_function)(v, w, Py_LT);
  if (res == NULL)
    return -1;

  int ok;
  if (PyBool_Check(res))
    ok = (res == Py_True);
  else
    ok = PyObject_IsTrue(res);
  Py_DECREF(res);
  return ok;
}

int unsafe_unicode_compare(PyObject* v, PyObject* w){
#ifdef Py_DEBUG
  assert(v->ob_type == &PyUnicode_Type && w->ob_type == &PyUnicode_Type &&
         PyUnicode_KIND(v) == PyUnicode_1BYTE_KIND && PyUnicode_KIND(w) == PyUnicode_1BYTE_KIND);
#endif

  int len = Py_MIN(PyUnicode_GET_LENGTH(v), PyUnicode_GET_LENGTH(w));
  return memcmp(PyUnicode_DATA(v), PyUnicode_DATA(w), len) < 0;
}

int unsafe_long_compare(PyObject *v, PyObject *w)
{
#ifdef Py_DEBUG
  assert(v->ob_type == &PyLong_Type && w->ob_type == &PyLong_Type &&
         Py_ABS(Py_SIZE(v)) <= 1 && Py_ABS(Py_SIZE(w)) <= 1);
#endif

  PyLongObject *vl, *wl;
  vl = (PyLongObject*)v;
  wl = (PyLongObject*)w;

  sdigit v0 = Py_SIZE(vl) == 0 ? 0 : (sdigit)vl->ob_digit[0];
  sdigit w0 = Py_SIZE(wl) == 0 ? 0 : (sdigit)wl->ob_digit[0];

  if (Py_SIZE(vl) < 0)
    v0 = -v0;
  if (Py_SIZE(wl) < 0)
    w0 = -w0;

  return v0 < w0;
}

int unsafe_float_compare(PyObject *v, PyObject *w)
{
#ifdef Py_DEBUG
  assert(v->ob_type == &PyFloat_Type && w->ob_type == &PyFloat_Type);
#endif

  return PyFloat_AS_DOUBLE(v) < PyFloat_AS_DOUBLE(w);
}


int unsafe_tuple_compare(PyObject* v, PyObject* w)
{
#ifdef Py_DEBUG
  assert(v->ob_type == PyTuple_Type && w->ob_type == PyTuple_Type &&
         Py_SIZE(v) > 0 && Py_SIZE(w) > 0);
#endif

  PyTupleObject *vt, *wt;
  Py_ssize_t i;
  Py_ssize_t vlen, wlen;

  vt = (PyTupleObject *)v;
  wt = (PyTupleObject *)w;

  int k;

  k = (*tuple_elem_compare)(vt->ob_item[0], wt->ob_item[0]);
  if (k < 0)
    return -1;
  if (k)
    return 1;

  vlen = Py_SIZE(vt);
  wlen = Py_SIZE(wt);

  if (vlen == 1 || wlen == 1)
    return 0;

  k = (*tuple_elem_compare)(wt->ob_item[0], vt->ob_item[0]);
  if (k < 0)
    return -1;
  if (k)
    return 0;

  for (i = 0; i < vlen && i < wlen; i++) {
    k = PyObject_RichCompareBool(vt->ob_item[i],
                                     wt->ob_item[i],
                                     Py_EQ);
    if (k < 0)
      return -1;
    if (!k)
      break;
  }

  if (i >= vlen || i >= wlen){
    return vlen <  wlen;
  }

  return PyObject_RichCompareBool(vt->ob_item[i],
                                  wt->ob_item[i],
                                  Py_LT);
}
