#include <Python.h>

typedef struct {
  PyListObject list;
} FastListObject;

static PyObject* fast_listsort(FastListObject *self_fastlist, PyObject *args, PyObject *kwds);
#include "fast_listsort.c"

static PyMethodDef FastList_methods[] = {
    {"fastsort", (PyCFunction)fast_listsort, METH_NOARGS,
     PyDoc_STR("fastsort method for FastLists")},
    {NULL,	NULL},
};

static int
FastList_init(FastListObject *self, PyObject *args, PyObject *kwds)
{
    if (PyList_Type.tp_init((PyObject *)self, args, kwds) < 0)
        return -1;
    return 0;
}

static PyTypeObject FastListType = {
  PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "fastlist.FastList",     /* tp_name */
    sizeof(FastListObject),  /* tp_basicsize */
    0,                       /* tp_itemsize */
    0,                       /* tp_dealloc */
    0,                       /* tp_print */
    0,                       /* tp_getattr */
    0,                       /* tp_setattr */
    0,                       /* tp_reserved */
    0,                       /* tp_repr */
    0,                       /* tp_as_number */
    0,                       /* tp_as_sequence */
    0,                       /* tp_as_mapping */
    0,                       /* tp_hash */
    0,                       /* tp_call */
    0,                       /* tp_str */
    0,                       /* tp_getattro */
    0,                       /* tp_setattro */
    0,                       /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT |
        Py_TPFLAGS_BASETYPE, /* tp_flags */
    0,                       /* tp_doc */
    0,                       /* tp_traverse */
    0,                       /* tp_clear */
    0,                       /* tp_richcompare */
    0,                       /* tp_weaklistoffset */
    0,                       /* tp_iter */
    0,                       /* tp_iternext */
    FastList_methods,          /* tp_methods */
    0,                       /* tp_members */
    0,                       /* tp_getset */
    0,                       /* tp_base */
    0,                       /* tp_dict */
    0,                       /* tp_descr_get */
    0,                       /* tp_descr_set */
    0,                       /* tp_dictoffset */
    (initproc)FastList_init,   /* tp_init */
    0,                       /* tp_alloc */
    0,                       /* tp_new */
};

static struct PyModuleDef moduledef = {
  PyModuleDef_HEAD_INIT,
  "fastlist",
  0,
  0,
  0,
  0,
  0,
  0,
  0
};

PyMODINIT_FUNC
PyInit_fastlist(void)
{
    PyObject* m = PyModule_Create(&moduledef);
    if (m == NULL)
      return NULL;

    FastListType.tp_base = &PyList_Type;
    if (PyType_Ready(&FastListType) < 0)
        return NULL;

    Py_INCREF(&FastListType);
    PyModule_AddObject(m, "FastList", (PyObject *) &FastListType);

    return m;
}
