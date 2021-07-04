#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <patchlevel.h>
#include <structmember.h>

#include "exttypes.h"
#include "moxie/platform.h"

// There's an excellent cheat sheet here:
// https://www.pythonsheets.com/notes/python-c-extensions.html

#define STRINGIZE_(x)                                                       #x
#define STRINGIZE(x)                                                        STRINGIZE_(x)
#define PYINIT_FUNC_(x)                                                     PyInit_ ## x
#define PYINIT_FUNC                                                         PYINIT_FUNC_(EXTENSION_NAME)
#define EXTENSION_VERSION_STRING                                                \
        STRINGIZE(EXTENSION_NAME) " " STRINGIZE(EXTENSION_VERSION_MAJOR) "." STRINGIZE(EXTENSION_VERSION_MINOR) "." STRINGIZE(EXTENSION_VERSION_REVISION) " (" PLATFORM_TARGET_COMPILER_NAME "," PLATFORM_TARGET_PLATFORM_NAME "," PLATFORM_TARGET_ARCHITECTURE_NAME "," PLATFORM_TARGET_ENDIANESS_NAME ") Python " PY_VERSION



PyDoc_STRVAR(
    module_docstring,
    "A library for host system resource management and loading of medical image data."
);

static PyObject*
version_string
(
    PyObject *self
)
{
    return PyUnicode_FromString(EXTENSION_VERSION_STRING);
}

static PyObject*
initialize_scheduler
(
    PyObject   *self,
    PyObject   *args,
    PyObject *kwargs
)
{
    PLATFORM_UNUSED_PARAM(self);
    PLATFORM_UNUSED_PARAM(args);
    PLATFORM_UNUSED_PARAM(kwargs);
    Py_RETURN_NONE;
}




static PyMethodDef gExtensionMethods[] = {
    {"version_string"      , (PyCFunction)version_string      , METH_NOARGS                 , PyDoc_STR("") },
    {"initialize_scheduler", (PyCFunction)initialize_scheduler, METH_VARARGS | METH_KEYWORDS, PyDoc_STR("") },
    {NULL, NULL,0, NULL} /* End of list */
};

static PyModuleDef gModuleDefinition = {
    .m_base    = PyModuleDef_HEAD_INIT    ,
    .m_name    = STRINGIZE(EXTENSION_NAME),
    .m_doc     = module_docstring         ,
    .m_size    = 0                        , /* Size of per-interpreter module state, or -1 if kept in global variables */
    .m_methods = gExtensionMethods
};

PyMODINIT_FUNC
PyInit_moxie
(
    void
)
{
    PyObject *mod = NULL;

    if (PyType_Ready(&MemoryMarkerType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&MemoryAllocatorType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&MemoryAllocationType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&JobQueueType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&JobContextType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&JobSchedulerType) < 0) {
        return NULL;
    }
    if ((mod = PyModule_Create(&gModuleDefinition)) == NULL) {
        return NULL;
    }
    if (PyModule_AddIntConstant(mod, "JOB_QUEUE_SIGNAL_CLEAR"    , JOB_QUEUE_SIGNAL_CLEAR    ) < 0) {
        return NULL;
    }
    if (PyModule_AddIntConstant(mod, "JOB_QUEUE_SIGNAL_TERMINATE", JOB_QUEUE_SIGNAL_TERMINATE) < 0) {
        return NULL;
    }
    if (PyModule_AddIntConstant(mod, "JOB_QUEUE_SIGNAL_USER"     , JOB_QUEUE_SIGNAL_USER     ) < 0) {
        return NULL;
    }
    Py_XINCREF(&MemoryMarkerType);
    Py_XINCREF(&MemoryAllocatorType);
    Py_XINCREF(&MemoryAllocationType);
    Py_XINCREF(&JobQueueType);
    Py_XINCREF(&JobContextType);
    Py_XINCREF(&JobSchedulerType);
    PyModule_AddObject(mod, "MemoryMarker"    , (PyObject*) &MemoryMarkerType);
    PyModule_AddObject(mod, "MemoryAllocator" , (PyObject*) &MemoryAllocatorType);
    PyModule_AddObject(mod, "MemoryAllocation", (PyObject*) &MemoryAllocationType);
    PyModule_AddObject(mod, "JobQueue"        , (PyObject*) &JobQueueType);
    PyModule_AddObject(mod, "JobContext"      , (PyObject*) &JobContextType);
    PyModule_AddObject(mod, "JobScheduler"    , (PyObject*) &JobSchedulerType);
    return mod;
}
