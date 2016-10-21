#define Py_DEBUG 1

//Prototypes
static int unicode_compare(PyObject *str1, PyObject *str2);
static int unicode_compare_eq(PyObject *str1, PyObject *str2);
static int long_compare(PyLongObject *a, PyLongObject *b);

//The compares
PyObject* (*tp_richcompare)(PyObject* v, PyObject* w, int op);
static int tp_richcompare_wrapper(PyObject* v, PyObject* w, int op){
#ifdef Py_DEBUG
  assert(op == Py_LT || op == Py_EQ);
  assert(v->ob_type->tp_richcompare == tp_richcompare && \
         w->ob_type->tp_richcompare == tp_richcompare);
#endif

  PyObject* k;
  if (!(k = tp_richcompare(v, w, op)))
      return -1; //translate NULL to -1 for error reporting

  return k == Py_True;
}

static int unsafe_general_unicode_compare(PyObject* v, PyObject* w, int op){
#ifdef Py_DEBUG
  assert(op == Py_LT || op == Py_EQ);
  assert(v->ob_type == &PyUnicode_Type && w->ob_type == &PyUnicode_Type);
#endif

  if (op == Py_LT) 
    return unicode_compare(v, w);
  else
    return unicode_compare_eq(v,w);
}
static int unsafe_latin_unicode_compare(PyObject* v, PyObject* w, int op){
#ifdef Py_DEBUG
  assert(op == Py_LT || op == Py_EQ);
  assert(v->ob_type == &PyUnicode_Type && w->ob_type == &PyUnicode_Type);
  assert(PyUnicode_KIND(v) == PyUnicode_1BYTE_KIND && \
         PyUnicode_KIND(w) == PyUnicode_1BYTE_KIND);
#endif

  int len = Py_MIN(PyUnicode_GET_LENGTH(v), PyUnicode_GET_LENGTH(w));
  if (op == Py_LT)
    return memcmp(PyUnicode_DATA(v), PyUnicode_DATA(w), len) < 0;
  else
    return memcmp(PyUnicode_DATA(v), PyUnicode_DATA(w), len) == 0;
}

static int unsafe_general_long_compare(PyObject* v, PyObject* w, int op)
{
#ifdef Py_DEBUG
  assert(op == Py_LT || op == Py_EQ);
  assert(v->ob_type == &PyLong_Type && w->ob_type == &PyLong_Type);
#endif

  if (op == Py_LT)
    return long_compare((PyLongObject*)v, (PyLongObject*)w) == -1;
  else
    return long_compare((PyLongObject*)v, (PyLongObject*)w) == 0;
}
static int unsafe_small_long_compare(PyLongObject *a, PyLongObject *b)
{
#ifdef Py_DEBUG
  assert(op == Py_LT || op == Py_EQ);
  assert(v->ob_type == &PyLong_Type && w->ob_type == &PyLong_Type);
  assert(Py_ABS(Py_SIZE(v)) <= 1 && Py_ABS(Py_SIZE(w)) <= 1)
#endif

  sdigit a0 = Py_SIZE(a) == 0 ? 0 : (sdigit)a->ob_digit[0];
  sdigit b0 = Py_SIZE(b) == 0 ? 0 : (sdigit)b->ob_digit[0];

  if (Py_SIZE(a) < 0)
    a0 = -a0;
  if (Py_SIZE(b) < 0)
    b0 = -b0;

  if (op == Py_LT)
    return a0 < b0;
  else
    return a0 == b0;
}

int unsafe_float_compare(PyObject *v, PyObject *w, int op)
{
#ifdef Py_DEBUG
  assert(op == Py_LT || op == Py_EQ);
  assert(v->ob_type == &PyFloat_Type && w->ob_type == &PyFloat_Type);
#endif

  if (op == Py_LT)
    return PyFloat_AS_DOUBLE(v) < PyFloat_AS_DOUBLE(w);
 else
   return PyFloat_AS_DOUBLE(v) == PyFloat_AS_DOUBLE(w);
}


(**elem_compare_funcs)(PyObject* v, PyObject* w, int op);
int unsafe_homogenous_tuple_compare(PyObject* v, PyObject* w, int op)
{
#ifdef Py_DEBUG
  assert(op == Py_LT || op == Py_EQ);
  assert(v->ob_type == PyTuple_Type && w->ob_type == PyTuple_Type);
  //assert(for all pairs of tuples (a,b), for all i, typeof(a[i]) == typeof(b[i]))
#endif

  PyTupleObject *vt, *wt;
  Py_ssize_t i;
  Py_ssize_t vlen, wlen;

  vt = (PyTupleObject *)v;
  wt = (PyTupleObject *)w;

  vlen = Py_SIZE(vt);
  wlen = Py_SIZE(wt);

  for (i = 0; i < vlen && i < wlen; i++) {
    int k = (*elem_compare_funcs[i])(vt->ob_item[i],
                                     wt->ob_item[i],
                                     Py_EQ);
    if (k < 0)
      return -1;
    if (!k)
      break;
  }

  if (i >= vlen || i >= wlen){
    if (op == Py_LT)
      return vlen <  wlen;
    else
      return vlen == wlen;
  }

  if (op == Py_LT)
    return (*elem_compare_funcs[i])(vt->ob_item[i],
                                          wt->ob_item[i],
                                          Py_LT);
  else
    return 0;
}
int unsafe_homogenous_tuple_compare(PyObject* v, PyObject* w, int op)
{
#ifdef Py_DEBUG
  assert(op == Py_LT || op == Py_EQ);
  assert(v->ob_type == PyTuple_Type && w->ob_type == PyTuple_Type);
#endif

  PyTupleObject *vt, *wt;
  Py_ssize_t i;
  Py_ssize_t vlen, wlen;

  vt = (PyTupleObject *)v;
  wt = (PyTupleObject *)w;

  vlen = Py_SIZE(vt);
  wlen = Py_SIZE(wt);

  for (i = 0; i < vlen && i < wlen; i++) {
    int k = PyObject_RichCompareBool(vt->ob_item[i],
                                     wt->ob_item[i],
                                     Py_EQ);
    if (k < 0)
      return -1;
    if (!k)
      break;
  }

  if (i >= vlen || i >= wlen){
    if (op == Py_LT)
      return vlen <  wlen;
    else
      return vlen == wlen;
  }

  if (op == Py_LT)
    return PyObject_RichCompareBool(vt->ob_item[i],
                                    wt->ob_item[i],
                                    Py_LT);
  else
    return 0;
}

/* This function assumes that str1 and str2 are readied by the caller. */

static int
unicode_compare(PyObject *str1, PyObject *str2)
{
#define COMPARE(TYPE1, TYPE2) \
    do { \
        TYPE1* p1 = (TYPE1 *)data1; \
        TYPE2* p2 = (TYPE2 *)data2; \
        TYPE1* end = p1 + len; \
        Py_UCS4 c1, c2; \
        for (; p1 != end; p1++, p2++) { \
            c1 = *p1; \
            c2 = *p2; \
            if (c1 != c2) \
                return (c1 < c2) ? -1 : 1; \
        } \
    } \
    while (0)

    int kind1, kind2;
    void *data1, *data2;
    Py_ssize_t len1, len2, len;

    kind1 = PyUnicode_KIND(str1);
    kind2 = PyUnicode_KIND(str2);
    data1 = PyUnicode_DATA(str1);
    data2 = PyUnicode_DATA(str2);
    len1 = PyUnicode_GET_LENGTH(str1);
    len2 = PyUnicode_GET_LENGTH(str2);
    len = Py_MIN(len1, len2);

    switch(kind1) {
    case PyUnicode_1BYTE_KIND:
    {
        switch(kind2) {
        case PyUnicode_1BYTE_KIND:
        {
            int cmp = memcmp(data1, data2, len);
            /* normalize result of memcmp() into the range [-1; 1] */
            if (cmp < 0)
                return -1;
            if (cmp > 0)
                return 1;
            break;
        }
        case PyUnicode_2BYTE_KIND:
            COMPARE(Py_UCS1, Py_UCS2);
            break;
        case PyUnicode_4BYTE_KIND:
            COMPARE(Py_UCS1, Py_UCS4);
            break;
        default:
            assert(0);
        }
        break;
    }
    case PyUnicode_2BYTE_KIND:
    {
        switch(kind2) {
        case PyUnicode_1BYTE_KIND:
            COMPARE(Py_UCS2, Py_UCS1);
            break;
        case PyUnicode_2BYTE_KIND:
        {
            COMPARE(Py_UCS2, Py_UCS2);
            break;
        }
        case PyUnicode_4BYTE_KIND:
            COMPARE(Py_UCS2, Py_UCS4);
            break;
        default:
            assert(0);
        }
        break;
    }
    case PyUnicode_4BYTE_KIND:
    {
        switch(kind2) {
        case PyUnicode_1BYTE_KIND:
            COMPARE(Py_UCS4, Py_UCS1);
            break;
        case PyUnicode_2BYTE_KIND:
            COMPARE(Py_UCS4, Py_UCS2);
            break;
        case PyUnicode_4BYTE_KIND:
        {
#if defined(HAVE_WMEMCMP) && SIZEOF_WCHAR_T == 4
            int cmp = wmemcmp((wchar_t *)data1, (wchar_t *)data2, len);
            /* normalize result of wmemcmp() into the range [-1; 1] */
            if (cmp < 0)
                return -1;
            if (cmp > 0)
                return 1;
#else
            COMPARE(Py_UCS4, Py_UCS4);
#endif
            break;
        }
        default:
            assert(0);
        }
        break;
    }
    default:
        assert(0);
    }

    if (len1 == len2)
        return 0;
    if (len1 < len2)
        return -1;
    else
        return 1;

#undef COMPARE
}

static int
unicode_compare_eq(PyObject *str1, PyObject *str2)
{
  int kind;
  void *data1, *data2;
  Py_ssize_t len;
  int cmp;

  len = PyUnicode_GET_LENGTH(str1);
  if (PyUnicode_GET_LENGTH(str2) != len)
    return 0;
  kind = PyUnicode_KIND(str1);
  if (PyUnicode_KIND(str2) != kind)
    return 0;
  data1 = PyUnicode_DATA(str1);
  data2 = PyUnicode_DATA(str2);

  cmp = memcmp(data1, data2, len * kind);
  return (cmp == 0);
}

#define Py_ABS(x) ((x) < 0 ? -(x) : (x))
static int
long_compare(PyLongObject *a, PyLongObject *b)
{
  Py_ssize_t sign;

  if (Py_SIZE(a) != Py_SIZE(b)) {
    sign = Py_SIZE(a) - Py_SIZE(b);
  }
  else {
    Py_ssize_t i = Py_ABS(Py_SIZE(a));
    while (--i >= 0 && a->ob_digit[i] == b->ob_digit[i])
      ;
    if (i < 0)
      sign = 0;
    else {
      sign = (sdigit)a->ob_digit[i] - (sdigit)b->ob_digit[i];
      if (Py_SIZE(a) < 0)
        sign = -sign;
    }
  }
  return sign < 0 ? -1 : sign > 0 ? 1 : 0;
}
#undef Py_ABS

