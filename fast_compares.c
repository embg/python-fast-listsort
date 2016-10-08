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
