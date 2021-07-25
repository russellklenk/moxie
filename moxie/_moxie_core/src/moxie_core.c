/**
 * moxie_core.c: Implements the interface for the _moxie_core native Python extension.
 */
#include "moxie_core.h"

#ifndef PYMOXIE_NO_ERROR_OUTPUT
#define PyMoxie_LogErrorN(_format)                                             \
    fprintf(stderr, _format)

#define PyMoxie_LogErrorV(_format, ...)                                        \
    fprintf(stderr, _format, __VA_ARGS__)
#else
#define PyMoxie_LogErrorN(_format)                                             
#define PyMoxie_LogErrorV(_format, ...)                                        
#endif

#define PyMoxie_ReadyType(_type_instance)                                      \
    if (ready_type_instance((void*)_type_instance) == -1) {                    \
        return -1;                                                             \
    }

#define PyMoxie_RegisterIntConstant(_module, _name, _value)                    \
    if (PyModule_AddIntConstant((PyObject*)_module, _name, (_value)) == -1) {  \
        return -1;                                                             \
    }

#define MEM_TAG_BUFFER_SIZE                                                    5


typedef struct mem_allocator_stats_t {                                         /* Information about a memory allocator instance. */
    size_t               watermark;                                            /* The high watermark value of the allocator. */
    size_t              bytes_free;                                            /* The number of bytes unused within the tail chunk. */
    size_t              bytes_used;                                            /* The total number of bytes allocated by the application. */
    size_t              bytes_lost;                                            /* The total number of bytes lost to internal fragmentation. */
    size_t             bytes_guard;                                            /* The total number of bytes allocated to guard pages. */
    size_t             bytes_total;                                            /* The total number of bytes reserved by the allocator. */
    size_t             chunk_count;                                            /* The total number of chunks in the arena. */
} mem_allocator_stats_t;


static void                          PyMoxie_MemoryMarker_dealloc(PyMoxie_MemoryMarker*);
static PyMoxie_MemoryMarker*         PyMoxie_MemoryMarker_new(PyTypeObject*, PyObject*, PyObject*);
static PyObject*                     PyMoxie_MemoryMarker_repr(PyMoxie_MemoryMarker*);
static PyObject*                     PyMoxie_MemoryMarker_str(PyMoxie_MemoryMarker*);

static void                          PyMoxie_MemoryAllocation_dealloc(PyMoxie_MemoryAllocation*);
static PyMoxie_MemoryAllocation*     PyMoxie_MemoryAllocation_new(PyTypeObject*, PyObject*, PyObject*);
static PyObject*                     PyMoxie_MemoryAllocation_repr(PyMoxie_MemoryAllocation*);
static PyObject*                     PyMoxie_MemoryAllocation_str(PyMoxie_MemoryAllocation*);
static int                           PyMoxie_MemoryAllocation_getbuffer(PyMoxie_MemoryAllocation*, Py_buffer*, int);

static void                          PyMoxie_InternalAllocator_dealloc(PyMoxie_InternalAllocator*);
static PyMoxie_InternalAllocator*    PyMoxie_InternalAllocator_new(PyTypeObject*, PyObject*, PyObject*);
static PyObject*                     PyMoxie_Create_Memory_Allocator(PyObject*, PyObject*, PyObject*);
static PyObject*                     PyMoxie_Allocate_Memory(PyObject*, PyObject*, PyObject*);
static PyObject*                     PyMoxie_Mark_Allocator(PyObject*, PyObject*);
static PyObject*                     PyMoxie_Reset_Allocator(PyObject*, PyObject*);
static PyObject*                     PyMoxie_Reset_Allocator_To_Marker(PyObject*, PyObject*, PyObject*);

static void                          PyMoxie_InternalJobQueue_dealloc(PyMoxie_InternalJobQueue*);
static PyMoxie_InternalJobQueue*     PyMoxie_InternalJobQueue_new(PyTypeObject*, PyObject*, PyObject*);
static PyObject*                     PyMoxie_Create_Job_Queue(PyObject*, PyObject*, PyObject*);
static PyObject*                     PyMoxie_Flush_Job_Queue(PyObject*, PyObject*);
static PyObject*                     PyMoxie_Signal_Job_Queue(PyObject*, PyObject*, PyObject*);
static PyObject*                     PyMoxie_Check_Job_Queue_Signal(PyObject*, PyObject*);

static void                          PyMoxie_InternalJobContext_dealloc(PyMoxie_InternalJobContext*);
static PyMoxie_InternalJobContext*   PyMoxie_InternalJobContext_new(PyTypeObject*, PyObject*, PyObject*);
static PyObject*                     PyMoxie_Acquire_JobContext(PyObject*, PyObject*, PyObject*);
static PyObject*                     PyMoxie_Release_JobContext(PyObject*, PyObject*);
static PyObject*                     PyMoxie_Create_Python_Job(PyObject*, PyObject*, PyObject*);
static PyObject*                     PyMoxie_Submit_Python_Job(PyObject*, PyObject*, PyObject*);
static PyObject*                     PyMoxie_Cancel_Job(PyObject*, PyObject*, PyObject*);
static PyObject*                     PyMoxie_Complete_Job(PyObject*, PyObject*, PyObject*);
static PyObject*                     PyMoxie_Wait_For_Job(PyObject*, PyObject*, PyObject*);
static PyObject*                     PyMoxie_Run_Next_Job(PyObject*, PyObject*);
static PyObject*                     PyMoxie_Run_Next_Job_No_Completion(PyObject*, PyObject*);

static void                          PyMoxie_InternalJobScheduler_dealloc(PyMoxie_InternalJobScheduler*);
static PyMoxie_InternalJobScheduler* PyMoxie_InternalJobScheduler_new(PyTypeObject*, PyObject*, PyObject*);
static PyObject*                     PyMoxie_Create_Job_Scheduler(PyObject*, PyObject*, PyObject*);
static PyObject*                     PyMoxie_Terminate_Job_Scheduler(PyObject*, PyObject*);
static PyObject*                     PyMoxie_Get_Job_Queue_Worker_Count(PyObject*, PyObject*, PyObject*);


static PyMemberDef MemoryMarker_members[] = {
    { 
        .name     = "allocator_tag", 
        .type     = T_OBJECT_EX,
        .offset   = PLATFORM_OFFSET_OF(PyMoxie_MemoryMarker, allocator_tag_int), 
        .flags    = READONLY, 
        .doc      = PyDoc_STR("The four-character code associated with the allocator from which the marker was obtained. Used for debugging.")
    },
    {
        .name     = "allocator_name",
        .type     = T_OBJECT_EX,
        .offset   = PLATFORM_OFFSET_OF(PyMoxie_MemoryMarker, allocator_name_str),
        .flags    = READONLY,
        .doc      = PyDoc_STR("The name of the allocator from which the marker was obtained. Used for debugging.")
    },
    {   /* Must be the last entry in the list. */
        .name     = NULL,
        .type     = 0,
        .offset   = 0,
        .flags    = 0,
        .doc      = NULL
    }
};

static PyMethodDef MemoryMarker_methods[] = {
    {   /* Must be the last entry in the list. */
        .ml_name  = NULL,
        .ml_meth  = NULL,
        .ml_flags = 0,
        .ml_doc   = NULL
    }
};

static PyMemberDef MemoryAllocation_members[] = {
    {
        .name     = "address",
        .type     = T_OBJECT_EX,
        .offset   = PLATFORM_OFFSET_OF(PyMoxie_MemoryAllocation, base_address_int),
        .flags    = READONLY,
        .doc      = PyDoc_STR("The virtual address of the first addressable byte of the memory block, or 0 if the memory block is invalid.")
    },
    {
        .name     = "length",
        .type     = T_OBJECT_EX,
        .offset   = PLATFORM_OFFSET_OF(PyMoxie_MemoryAllocation, length_int),
        .flags    = READONLY,
        .doc      = PyDoc_STR("The capacity of the memory block, in bytes.")
    },
    {
        .name     = "alignment",
        .type     = T_OBJECT_EX,
        .offset   = PLATFORM_OFFSET_OF(PyMoxie_MemoryAllocation, alignment_int),
        .flags    = READONLY,
        .doc      = PyDoc_STR("The requested alignment of the base address, in bytes.")
    },
    {
        .name     = "readonly",
        .type     = T_OBJECT_EX,
        .offset   = PLATFORM_OFFSET_OF(PyMoxie_MemoryAllocation, readonly_bool),
        .flags    = READONLY,
        .doc      = PyDoc_STR("True if the memory block is read-only.")
    },
    {
        .name     = "allocator_tag",
        .type     = T_OBJECT_EX,
        .offset   = PLATFORM_OFFSET_OF(PyMoxie_MemoryAllocation, allocator_tag_int),
        .flags    = READONLY,
        .doc      = PyDoc_STR("The four-character code associated with the allocator from which the memory block was acquired. Used for debugging.")
    },
    {
        .name     = "allocator_name",
        .type     = T_OBJECT_EX,
        .offset   = PLATFORM_OFFSET_OF(PyMoxie_MemoryAllocation, allocator_name_str),
        .flags    = READONLY,
        .doc      = PyDoc_STR("The name of the allocator from which the memory block was acquired. Used for debugging.")
    },
    {   /* Must be the last entry in the list. */
        .name     = NULL,
        .type     = 0,
        .offset   = 0,
        .flags    = 0,
        .doc      = NULL
    }
};

static PyMethodDef MemoryAllocation_methods[] = {
    {   /* Must be the last entry in the list. */
        .ml_name  = NULL,
        .ml_meth  = NULL,
        .ml_flags = 0,
        .ml_doc   = NULL
    }
};

static PyBufferProcs MemoryAllocation_as_buffer = {
    .bf_getbuffer     =(getbufferproc    ) PyMoxie_MemoryAllocation_getbuffer,
    .bf_releasebuffer =(releasebufferproc) NULL
};

static PyMemberDef InternalAllocator_members[] = {
    {
        .name     = "name",
        .type     = T_OBJECT_EX,
        .offset   = PLATFORM_OFFSET_OF(PyMoxie_InternalAllocator, allocator_name_str),
        .flags    = READONLY,
        .doc      = PyDoc_STR("The name of the allocator. Used for debugging.")
    },
    { 
        .name     = "tag", 
        .type     = T_OBJECT_EX,
        .offset   = PLATFORM_OFFSET_OF(PyMoxie_InternalAllocator, allocator_tag_int), 
        .flags    = READONLY, 
        .doc      = PyDoc_STR("The four-character code identifying the allocator. Used for debugging.")
    },
    { 
        .name     = "page_size", 
        .type     = T_OBJECT_EX,
        .offset   = PLATFORM_OFFSET_OF(PyMoxie_InternalAllocator, page_size_int), 
        .flags    = READONLY, 
        .doc      = PyDoc_STR("The operating system page size, in bytes.")
    },
    { 
        .name     = "growable", 
        .type     = T_OBJECT_EX,
        .offset   = PLATFORM_OFFSET_OF(PyMoxie_InternalAllocator, growable_bool), 
        .flags    = READONLY, 
        .doc      = PyDoc_STR("True if the maximum allocator size can increase beyond the initial capacity.")
    },
    {   /* Must be the last entry in the list. */
        .name     = NULL,
        .type     = 0,
        .offset   = 0,
        .flags    = 0,
        .doc      = NULL
    }
};

static PyMethodDef InternalAllocator_methods[] = {
    {   /* Must be the last entry in the list. */
        .ml_name  = NULL,
        .ml_meth  = NULL,
        .ml_flags = 0,
        .ml_doc   = NULL
    }
};

static PyMemberDef InternalJobQueue_members[] = {
    {   /* Must be the last entry in the list. */
        .name     = NULL,
        .type     = 0,
        .offset   = 0,
        .flags    = 0,
        .doc      = NULL
    }
};

static PyMethodDef InternalJobQueue_methods[] = {
    {   /* Must be the last entry in the list. */
        .ml_name  = NULL,
        .ml_meth  = NULL,
        .ml_flags = 0,
        .ml_doc   = NULL
    }
};

static PyMemberDef InternalJobContext_members[] = {
    {   /* Must be the last entry in the list. */
        .name     = NULL,
        .type     = 0,
        .offset   = 0,
        .flags    = 0,
        .doc      = NULL
    }
};

static PyMethodDef InternalJobContext_methods[] = {
    {   /* Must be the last entry in the list. */
        .ml_name  = NULL,
        .ml_meth  = NULL,
        .ml_flags = 0,
        .ml_doc   = NULL
    }
};

static PyMemberDef InternalJobScheduler_members[] = {
    {   /* Must be the last entry in the list. */
        .name     = NULL,
        .type     = 0,
        .offset   = 0,
        .flags    = 0,
        .doc      = NULL
    }
};

static PyMethodDef InternalJobScheduler_methods[] = {
    {   /* Must be the last entry in the list. */
        .ml_name  = NULL,
        .ml_meth  = NULL,
        .ml_flags = 0,
        .ml_doc   = NULL
    }
};

static PyMethodDef PyMoxie_Core_ExtensionFunctions[] = {
    {
        .ml_name  = "create_memory_allocator",
        .ml_meth  =(PyCFunction) PyMoxie_Create_Memory_Allocator,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc   = PyDoc_STR("Create a new arena memory allocator (or sub-allocator).")
    },
    {
        .ml_name  = "allocate_memory",
        .ml_meth  =(PyCFunction) PyMoxie_Allocate_Memory,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc   = PyDoc_STR("Allocate memory with a specific alignment from a memory arena.")
    },
    {
        .ml_name  = "create_allocator_marker",
        .ml_meth  =(PyCFunction) PyMoxie_Mark_Allocator,
        .ml_flags = METH_VARARGS,
        .ml_doc   = PyDoc_STR("Create a marker representing the current state of a memory arena.")
    },
    {
        .ml_name  = "reset_memory_allocator",
        .ml_meth  =(PyCFunction) PyMoxie_Reset_Allocator,
        .ml_flags = METH_VARARGS,
        .ml_doc   = PyDoc_STR("Reset the state of a memory arena, invalidating all allocations.")
    },
    {
        .ml_name  = "reset_memory_allocator_to_marker",
        .ml_meth  =(PyCFunction) PyMoxie_Reset_Allocator_To_Marker,
        .ml_flags = METH_VARARGS,
        .ml_doc   = PyDoc_STR("Reset the state of a memory arena to a previously obtained marker, invalidating all allocations made since the marker was obtained.")
    },
    {
        .ml_name  = "create_job_queue",
        .ml_meth  =(PyCFunction) PyMoxie_Create_Job_Queue,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc   = PyDoc_STR("Allocate and initialize an empty, waitable job queue object.")
    },
    {
        .ml_name  = "flush_job_queue",
        .ml_meth  =(PyCFunction) PyMoxie_Flush_Job_Queue,
        .ml_flags = METH_VARARGS,
        .ml_doc   = PyDoc_STR("Flush the queue and wake all waiting producer threads.")
    },
    {
        .ml_name  = "signal_job_queue",
        .ml_meth  =(PyCFunction) PyMoxie_Signal_Job_Queue,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc   = PyDoc_STR("Signal all waiters on a job queue to wake up for some event such as process shutdown.")
    },
    {
        .ml_name  = "check_job_queue_signal",
        .ml_meth  =(PyCFunction) PyMoxie_Check_Job_Queue_Signal,
        .ml_flags = METH_VARARGS,
        .ml_doc   = PyDoc_STR("Retrieve the signal value for a job queue. If the queue is not signaled, the return value is _moxie_core.JOB_QUEUE_SIGNAL_CLEAR (0).")
    },
    {
        .ml_name  = "create_job_scheduler",
        .ml_meth  =(PyCFunction) PyMoxie_Create_Job_Scheduler,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc   = PyDoc_STR("Allocate and initialize a job scheduler instance.")
    },
    {
        .ml_name  = "terminate_job_scheduler",
        .ml_meth  =(PyCFunction) PyMoxie_Terminate_Job_Scheduler,
        .ml_flags = METH_VARARGS,
        .ml_doc   = PyDoc_STR("Wake up any waiting threads and send _moxie_core.JOB_QUEUE_SIGNAL_TERMINATE to their wait queue.")
    },
    {
        .ml_name  = "get_worker_count_for_queue",
        .ml_meth  =(PyCFunction) PyMoxie_Get_Job_Queue_Worker_Count,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc   = PyDoc_STR("Retrieve the number of threads that wait on a given job queue.") 
    },
    {
        .ml_name  = "acquire_job_context",
        .ml_meth  =(PyCFunction) PyMoxie_Acquire_JobContext,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc   = PyDoc_STR("Allocate and bind a job management context to a thread and wait queue.")
    },
    {
        .ml_name  = "release_job_context",
        .ml_meth  =(PyCFunction) PyMoxie_Release_JobContext,
        .ml_flags = METH_VARARGS,
        .ml_doc   = PyDoc_STR("Return a job management context to the free pool when the associated thread terminates or the context is no longer needed.")
    },
    {
        .ml_name  = "create_python_job",
        .ml_meth  =(PyCFunction) PyMoxie_Create_Python_Job,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc   = PyDoc_STR("Allocate a job identifier for a job implemented in Python.")
    },
    {
        .ml_name  = "submit_python_job",
        .ml_meth  =(PyCFunction) PyMoxie_Submit_Python_Job,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc   = PyDoc_STR("Submit or cancel a job implemented in Python.")
    },
    {
        .ml_name  = "cancel_job",
        .ml_meth  =(PyCFunction) PyMoxie_Cancel_Job,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc   = PyDoc_STR("Attempt to cancel a previously submitted job.")
    },
    {
        .ml_name  = "complete_job",
        .ml_meth  =(PyCFunction) PyMoxie_Complete_Job,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc   = PyDoc_STR("Indicate that a particular job has finished execution.")
    },
    {
        .ml_name  = "wait_for_job",
        .ml_meth  =(PyCFunction) PyMoxie_Wait_For_Job,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc   = PyDoc_STR("Execute ready-to-run jobs while waiting for a specific job to complete.")
    },
    {
        .ml_name  = "run_next_job",
        .ml_meth  =(PyCFunction) PyMoxie_Run_Next_Job,
        .ml_flags = METH_VARARGS,
        .ml_doc   = PyDoc_STR("Wait for a job to become ready-to-run, execute the job, and complete the job.")
    },
    {
        .ml_name  = "run_next_job_no_completion",
        .ml_meth  =(PyCFunction) PyMoxie_Run_Next_Job_No_Completion,
        .ml_flags = METH_VARARGS,
        .ml_doc   = PyDoc_STR("Wait for a job to become ready-to-run and then execute the job, but do not complete the job.")
    },
    {   /* Must be the last entry in the list. */
        .ml_name  = NULL,
        .ml_meth  = NULL,
        .ml_flags = 0,
        .ml_doc   = NULL
    }
};

static PyModuleDef PyMoxie_Core_ModuleDefinition = {
    .m_base       = PyModuleDef_HEAD_INIT,
    .m_name       = "moxie._moxie_core",
    .m_doc        = PyDoc_STR("The C extension module providing access to the underlying platform memory management and CPU execution resources."),
    .m_size       = 0, /* Size of per-interpreter module state, in bytes, or -1 if kept in global variables. */
    .m_methods    = PyMoxie_Core_ExtensionFunctions
};


PyTypeObject PyMoxie_MemoryMarkerType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "moxie._moxie_core.MemoryMarker",
    .tp_doc       = PyDoc_STR("Stores the state of a memory allocator at a specific point in time."),
    .tp_basicsize = sizeof(PyMoxie_MemoryMarker),
    .tp_itemsize  = 0,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       =(newfunc   )PyMoxie_MemoryMarker_new,
    .tp_dealloc   =(destructor)PyMoxie_MemoryMarker_dealloc,
    .tp_repr      =(reprfunc  )PyMoxie_MemoryMarker_repr,
    .tp_str       =(reprfunc  )PyMoxie_MemoryMarker_str,
    .tp_members   = MemoryMarker_members,
    .tp_methods   = MemoryMarker_methods
};

PyTypeObject PyMoxie_MemoryAllocationType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "moxie._moxie_core.MemoryAllocation",
    .tp_doc       = PyDoc_STR("Stores the attributes of a memory allocation."),
    .tp_basicsize = sizeof(PyMoxie_MemoryAllocation),
    .tp_itemsize  = 0,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       =(newfunc   )PyMoxie_MemoryAllocation_new,
    .tp_dealloc   =(destructor)PyMoxie_MemoryAllocation_dealloc,
    .tp_repr      =(reprfunc  )PyMoxie_MemoryAllocation_repr,
    .tp_str       =(reprfunc  )PyMoxie_MemoryAllocation_str,
    .tp_members   = MemoryAllocation_members,
    .tp_methods   = MemoryAllocation_methods,
    .tp_as_buffer =&MemoryAllocation_as_buffer
};

PyTypeObject PyMoxie_InternalAllocatorType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "moxie._moxie_core.InternalAllocator",
    .tp_doc       = PyDoc_STR("Carries state associated with an arena memory allocator."),
    .tp_basicsize = sizeof(PyMoxie_InternalAllocator),
    .tp_itemsize  = 0,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       =(newfunc   )PyMoxie_InternalAllocator_new,
    .tp_dealloc   =(destructor)PyMoxie_InternalAllocator_dealloc,
    .tp_members   = InternalAllocator_members,
    .tp_methods   = InternalAllocator_methods
};

PyTypeObject PyMoxie_InternalJobQueueType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "moxie._moxie_core.InternalJobQueue",
    .tp_doc       = PyDoc_STR("Carries state associated with a waitable job queue."),
    .tp_basicsize = sizeof(PyMoxie_InternalJobQueue),
    .tp_itemsize  = 0,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       =(newfunc   )PyMoxie_InternalJobQueue_new,
    .tp_dealloc   =(destructor)PyMoxie_InternalJobQueue_dealloc,
    .tp_members   = InternalJobQueue_members,
    .tp_methods   = InternalJobQueue_methods
};

PyTypeObject PyMoxie_InternalJobContextType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "moxie._moxie_core.InternalJobContext",
    .tp_doc       = PyDoc_STR("Carries state associated with a job management context."),
    .tp_basicsize = sizeof(PyMoxie_InternalJobContext),
    .tp_itemsize  = 0,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       =(newfunc   )PyMoxie_InternalJobContext_new,
    .tp_dealloc   =(destructor)PyMoxie_InternalJobContext_dealloc,
    .tp_members   = InternalJobContext_members,
    .tp_methods   = InternalJobContext_methods
};

PyTypeObject PyMoxie_InternalJobSchedulerType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "moxie._moxie_core.InternalJobScheduler",
    .tp_doc       = PyDoc_STR("Carries state associated with a job scheduler."),
    .tp_basicsize = sizeof(PyMoxie_InternalJobScheduler),
    .tp_itemsize  = 0,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       =(newfunc   )PyMoxie_InternalJobScheduler_new,
    .tp_dealloc   =(destructor)PyMoxie_InternalJobScheduler_dealloc,
    .tp_members   = InternalJobScheduler_members,
    .tp_methods   = InternalJobScheduler_methods
};


/**
 * Helper routine to call PyType_Ready and retain a reference for a specific type object.
 * @param _type_instance The Python type object address.
 * @return Zero of the operation is successful, or -1 if an error occurred.
 */
static int
ready_type_instance
(
    void *_type_instance
)
{   assert(_type_instance != NULL && "Expected non-NULL _type_instance argument");
    if (PyType_Ready((PyTypeObject*)_type_instance) >= 0) {
        Py_INCREF((PyObject*)_type_instance);
        return 0;
    } else {
        assert(0 && "PyType_Ready failed");
        return -1;
    }
}

/**
 * Ready the types exported by the module, populating any unset struct fields in the type definitions.
 * @param _moxie_core The Python _moxie_core module object reference.
 * @return Zero if all types are successfully readied, or -1 if an error occurred.
 */
static int
extension_ready_types
(
    PyObject *_moxie_core
)
{   assert(_moxie_core != NULL && "Expected non-NULL _moxie_core module argument");
    PyMoxie_ReadyType(&PyMoxie_MemoryMarkerType);
    PyMoxie_ReadyType(&PyMoxie_MemoryAllocationType);
    PyMoxie_ReadyType(&PyMoxie_InternalAllocatorType);
    PyMoxie_ReadyType(&PyMoxie_InternalJobQueueType);
    PyMoxie_ReadyType(&PyMoxie_InternalJobContextType);
    PyMoxie_ReadyType(&PyMoxie_InternalJobSchedulerType);
    return 0;
}

/**
 * Register constants exported by the module.
 * @param _moxie_core The Python _moxie_core module object reference.
 * @return Zero if all constants are successfully registered, or -1 if an error occurred.
 */
static int
extension_register_constants
(
    PyObject *_moxie_core
)
{   assert(_moxie_core != NULL && "Expected non-NULL _moxie_core module argument");
    PyMoxie_RegisterIntConstant(_moxie_core, "MEM_ALLOCATION_FLAGS_NONE"   , MEM_ALLOCATION_FLAGS_NONE);
    PyMoxie_RegisterIntConstant(_moxie_core, "MEM_ALLOCATION_FLAG_LOCAL"   , MEM_ALLOCATION_FLAG_LOCAL);
    PyMoxie_RegisterIntConstant(_moxie_core, "MEM_ALLOCATION_FLAG_SHARED"  , MEM_ALLOCATION_FLAG_SHARED);
    PyMoxie_RegisterIntConstant(_moxie_core, "MEM_ALLOCATION_FLAG_HEAP"    , MEM_ALLOCATION_FLAG_HEAP);
    PyMoxie_RegisterIntConstant(_moxie_core, "MEM_ALLOCATION_FLAG_VIRTUAL" , MEM_ALLOCATION_FLAG_VIRTUAL);
    PyMoxie_RegisterIntConstant(_moxie_core, "MEM_ALLOCATION_FLAG_EXTERNAL", MEM_ALLOCATION_FLAG_EXTERNAL);
    PyMoxie_RegisterIntConstant(_moxie_core, "MEM_ALLOCATION_FLAG_GROWABLE", MEM_ALLOCATION_FLAG_GROWABLE);
    PyMoxie_RegisterIntConstant(_moxie_core, "MEM_ACCESS_FLAGS_NONE"       , MEM_ACCESS_FLAGS_NONE);
    PyMoxie_RegisterIntConstant(_moxie_core, "MEM_ACCESS_FLAG_READ"        , MEM_ACCESS_FLAG_READ);
    PyMoxie_RegisterIntConstant(_moxie_core, "MEM_ACCESS_FLAG_WRITE"       , MEM_ACCESS_FLAG_WRITE);
    PyMoxie_RegisterIntConstant(_moxie_core, "MEM_ACCESS_FLAG_RDWR"        , MEM_ACCESS_FLAG_RDWR);
    PyMoxie_RegisterIntConstant(_moxie_core, "JOB_ID_INVALID"              , JOB_ID_INVALID);
    PyMoxie_RegisterIntConstant(_moxie_core, "JOB_SUBMIT_RUN"              , JOB_SUBMIT_RUN);
    PyMoxie_RegisterIntConstant(_moxie_core, "JOB_SUBMIT_CANCEL"           , JOB_SUBMIT_CANCEL);
    PyMoxie_RegisterIntConstant(_moxie_core, "JOB_SUBMIT_SUCCESS"          , JOB_SUBMIT_SUCCESS);
    PyMoxie_RegisterIntConstant(_moxie_core, "JOB_SUBMIT_INVALID_JOB"      , JOB_SUBMIT_INVALID_JOB);
    PyMoxie_RegisterIntConstant(_moxie_core, "JOB_SUBMIT_TOO_MANY_WAITERS" , JOB_SUBMIT_TOO_MANY_WAITERS);
    PyMoxie_RegisterIntConstant(_moxie_core, "JOB_STATE_UNINITALIZED"      , JOB_STATE_UNINITIALIZED);
    PyMoxie_RegisterIntConstant(_moxie_core, "JOB_STATE_NOT_SUBMITTED"     , JOB_STATE_NOT_SUBMITTED);
    PyMoxie_RegisterIntConstant(_moxie_core, "JOB_STATE_NOT_READY"         , JOB_STATE_NOT_READY);
    PyMoxie_RegisterIntConstant(_moxie_core, "JOB_STATE_READY"             , JOB_STATE_READY);
    PyMoxie_RegisterIntConstant(_moxie_core, "JOB_STATE_RUNNING"           , JOB_STATE_RUNNING);
    PyMoxie_RegisterIntConstant(_moxie_core, "JOB_STATE_COMPLETED"         , JOB_STATE_COMPLETED);
    PyMoxie_RegisterIntConstant(_moxie_core, "JOB_STATE_CANCELED"          , JOB_STATE_CANCELED);
    PyMoxie_RegisterIntConstant(_moxie_core, "JOB_QUEUE_SIGNAL_CLEAR"      , JOB_QUEUE_SIGNAL_CLEAR);
    PyMoxie_RegisterIntConstant(_moxie_core, "JOB_QUEUE_SIGNAL_TERMINATE"  , JOB_QUEUE_SIGNAL_TERMINATE);
    PyMoxie_RegisterIntConstant(_moxie_core, "JOB_QUEUE_SIGNAL_USER"       , JOB_QUEUE_SIGNAL_USER);
    return 0;
}

/**
 * Convert a mem_tag_t (four character code) to a nul-terminated ASCII string.
 * @param dst The destination buffer, which must be at least 5 bytes long.
 * @param tag The tag to format.
 * @return A pointer to the start of the destination buffer.
 */
static char*
mem_tag_to_ascii
(
    char     *dst,
    mem_tag_t tag
)
{
    char *ptr = &dst[0];
    *(uint32_t*) ptr = tag;
    dst[4] = '\0';
    return dst;
}

/**
 * Compute memory usage statistics for a given memory allocator.
 * @param stats The structure to populate.
 * @param alloc The allocator to inspect.
 */
static void
mem_allocator_stats
(
    struct mem_allocator_stats_t *stats,
    struct mem_allocator_t       *alloc
)
{
    struct mem_chunk_t *iter = NULL;
    size_t             nfree = 0;
    size_t             nused = 0;
    size_t             nlost = 0;
    size_t             guard = 0;
    size_t             total = 0;
    size_t            chunks = 0;

    if (stats == NULL) {
        return;
    }
    if (alloc == NULL) {
        stats->watermark   = 0;
        stats->bytes_free  = 0;
        stats->bytes_used  = 0;
        stats->bytes_lost  = 0;
        stats->bytes_guard = 0;
        stats->bytes_total = 0;
        stats->chunk_count = 0;
        return;
    }
    iter = alloc->head;
    while (iter != NULL) {
        chunks  += 1;
        nused   += mem_chunk_bytes_used(iter);
        guard   += alloc->guard_size;
        total   += mem_chunk_bytes_total(iter);
        if (iter  != alloc->tail) {
            nlost += mem_chunk_bytes_free(iter);
            iter   = iter->next;
        } else {
            nfree += mem_chunk_bytes_free(iter);
            break;
        }
    }
    stats->watermark   = alloc->high_watermark;
    stats->bytes_free  = nfree;
    stats->bytes_used  = nused;
    stats->bytes_lost  = nlost;
    stats->bytes_guard = guard;
    stats->bytes_total = total;
    stats->chunk_count = chunks;
}

/**
 * The job entry point for jobs implemented in Python.
 * The Global Interpreter Lock is held while calling back into the Python code.
 * This is not exceptionally efficient, as several allocations are made - but if you want maximum efficiency, implement your jobs in something other than Python.
 * @param context The job context associated with the thread executing the job.
 * @param job Data associated with the job being executed.
 * @param call_type One of the values of the job_call_type_e enumeration indicating the stage being run.
 * @return An application-specific exit code, where zero is reserved to mean success.
 */
static int32_t
python_job_main
(
    struct job_context_t *context,
    struct job_descriptor_t  *job,
    int32_t             call_type
)
{
    PyMoxie_PythonJobState *data =(PyMoxie_PythonJobState*) job->data;
    PyObject             *result = NULL;
    PyObject              *thrid = NULL;
    PyObject              *jobid = NULL;
    PyObject             *jobctx = NULL;
    int32_t               retval = 0;

    if (call_type == JOB_CALL_TYPE_EXECUTE) {
        /* Acquire the GIL while calling back into Python code. */
        PyGILState_STATE   GIL_state = PyGILState_Ensure();
        /* Create a temporary representing the job ID.
         * Ownership is transferred to the kwargs dict. */
        jobid  = PyLong_FromUnsignedLong((unsigned long) job->id);
        /* Resolve the native context into a JobContext instance. */
        thrid  = PyLong_FromVoidPtr((void*) context->thrid);
        jobctx = PyDict_GetItem(data->id_to_ctx, thrid);
        Py_DECREF(thrid); thrid = NULL;
        if (data->kwargs == NULL || data->kwargs == Py_None) {
            data->kwargs  = Py_BuildValue("{s:O,s:O}", "jobctx", jobctx, "job", jobid);
        } else {
            PyDict_SetItemString(data->kwargs, "jobctx", jobctx);
            PyDict_SetItemString(data->kwargs, "job"   , jobid);
        }
        if (jobctx != NULL) {
            result  = PyObject_Call(data->callable, data->args, data->kwargs);
            retval  = PyLong_AsLong(result);
            Py_DECREF(result); result = NULL;
        } else {
            PyErr_Format(PyExc_RuntimeError, "Failed to find JobContext for thread ID %zu", context->thrid);
        }
        PyGILState_Release(GIL_state);
    }
    if (call_type == JOB_CALL_TYPE_CLEANUP) {
        PyGILState_STATE   GIL_state = PyGILState_Ensure();
        Py_DECREF(data->id_to_ctx); data->id_to_ctx = NULL;
        Py_DECREF(data->callable ); data->callable  = NULL;
        Py_DECREF(data->args     ); data->args      = NULL;
        Py_DECREF(data->kwargs   ); data->kwargs    = NULL;
        PyGILState_Release(GIL_state);
    }
    return retval;
}

static void
PyMoxie_MemoryMarker_dealloc
(
    PyMoxie_MemoryMarker *self
)
{
    if (self != NULL) {
        Py_XDECREF(self->allocator_name_str);
        Py_XDECREF(self->allocator_tag_int);
        Py_TYPE(self)->tp_free((PyObject*) self);
    }
}

static void
PyMoxie_MemoryAllocation_dealloc
(
    PyMoxie_MemoryAllocation *self
)
{
    if (self != NULL) {
        Py_XDECREF(self->readonly_bool);
        Py_XDECREF(self->allocator_tag_int);
        Py_XDECREF(self->allocator_name_str);
        Py_XDECREF(self->alignment_int);
        Py_XDECREF(self->length_int);
        Py_XDECREF(self->base_address_int);
        Py_TYPE(self)->tp_free((PyObject*) self);
    }
}

static void
PyMoxie_InternalAllocator_dealloc
(
    PyMoxie_InternalAllocator *self
)
{
    if (self != NULL) {
        mem_allocator_delete(&self->allocator);
        Py_XDECREF(self->growable_bool);
        Py_XDECREF(self->page_size_int);
        Py_XDECREF(self->allocator_tag_int);
        Py_XDECREF(self->allocator_name_str);
        Py_TYPE(self)->tp_free((PyObject*) self);
    }
}

static void
PyMoxie_InternalJobQueue_dealloc
(
    PyMoxie_InternalJobQueue *self
)
{
    if (self != NULL) {
        job_queue_delete(self->state);
        Py_TYPE(self)->tp_free((PyObject*) self);
    }
}

static void
PyMoxie_InternalJobContext_dealloc
(
    PyMoxie_InternalJobContext *self
)
{
    if (self != NULL) {
        if (self->state != NULL && self->sched != NULL && self->sched->state != NULL) {
            job_scheduler_release_context(self->sched->state, self->state);
        }
        Py_XDECREF(self->sched);
        Py_XDECREF(self->queue);
        Py_TYPE(self)->tp_free((PyObject*) self);
    }
}

static void
PyMoxie_InternalJobScheduler_dealloc
(
    PyMoxie_InternalJobScheduler *self
)
{
    if (self != NULL) {
        job_scheduler_terminate(self->state);
        job_scheduler_delete(self->state);
        Py_TYPE(self)->tp_free((PyObject*) self);
    }
}

static PyMoxie_MemoryMarker*
PyMoxie_MemoryMarker_new
(
    PyTypeObject * type,
    PyObject     * Py_UNUSED(args),
    PyObject     * Py_UNUSED(kwargs)
)
{
    PyMoxie_MemoryMarker *self = NULL;

    if ((self = (PyMoxie_MemoryMarker*) type->tp_alloc(type, 0)) == NULL) {
        PyMoxie_LogErrorN("_moxie_core: tp_alloc failed for MemoryMarker.\n");
        return NULL;
    }
    self->allocator_name_str = PyMoxie_Retain(Py_None);
    self->allocator_tag_int  = PyMoxie_Retain(Py_None);
    self->marker.chunk       = NULL;
    self->marker.offset      = 0;
    self->marker.tag         = 0;
    self->marker.version     = 0;
    return self;
}

static PyMoxie_MemoryAllocation*
PyMoxie_MemoryAllocation_new
(
    PyTypeObject * type,
    PyObject     * Py_UNUSED(args),
    PyObject     * Py_UNUSED(kwargs)
)
{
    PyMoxie_MemoryAllocation *self = NULL;

    if ((self = (PyMoxie_MemoryAllocation*) type->tp_alloc(type, 0)) == NULL) {
        PyMoxie_LogErrorN("_moxie_core: tp_alloc failed for MemoryAllocation.\n");
        return NULL;
    }
    self->base_address_int   = PyLong_FromVoidPtr(NULL);
    self->length_int         = PyLong_FromUnsignedLongLong(0ull);
    self->alignment_int      = PyLong_FromUnsignedLong(1ul);
    self->allocator_name_str = PyMoxie_Retain(Py_None);
    self->allocator_tag_int  = PyMoxie_Retain(Py_None);
    self->readonly_bool      = PyMoxie_Retain(Py_True);
    self->base_address       = NULL;
    self->byte_length        = 0;
    self->tag                = 0;
    return self;
}

static PyMoxie_InternalAllocator*
PyMoxie_InternalAllocator_new
(
    PyTypeObject * type,
    PyObject     * Py_UNUSED(args),
    PyObject     * Py_UNUSED(kwargs)
)
{
    PyMoxie_InternalAllocator *self = NULL;

    if ((self = (PyMoxie_InternalAllocator*) type->tp_alloc(type, 0)) == NULL) {
        PyMoxie_LogErrorN("_moxie_core: tp_alloc failed for InternalAllocator.\n");
        return NULL;
    }
    self->allocator_name_str = PyMoxie_Retain(Py_None);
    self->allocator_tag_int  = PyMoxie_Retain(Py_None);
    self->page_size_int      = PyLong_FromSize_t(0);
    self->growable_bool      = PyMoxie_Retain(Py_False);
    mem_zero(&self->allocator, sizeof(mem_allocator_t));
    return self;
}

static PyMoxie_InternalJobQueue*
PyMoxie_InternalJobQueue_new
(
    PyTypeObject * type,
    PyObject     * Py_UNUSED(args),
    PyObject     * Py_UNUSED(kwargs)
)
{
    PyMoxie_InternalJobQueue *self = NULL;

    if ((self = (PyMoxie_InternalJobQueue*) type->tp_alloc(type, 0)) == NULL) {
        PyMoxie_LogErrorN("_moxie_core: tp_alloc failed for InternalJobQueue.\n");
        return NULL;
    }
    self->state = NULL;
    return self;
}

static PyMoxie_InternalJobContext*
PyMoxie_InternalJobContext_new
(
    PyTypeObject * type,
    PyObject     * Py_UNUSED(args),
    PyObject     * Py_UNUSED(kwargs)
)
{
    PyMoxie_InternalJobContext *self = NULL;

    if ((self = (PyMoxie_InternalJobContext*) type->tp_alloc(type, 0)) == NULL) {
        PyMoxie_LogErrorN("_moxie_core: tp_alloc failed for InternalJobContext.\n");
        return NULL;
    }
    self->state = NULL;
    self->queue = NULL;
    self->sched = NULL;
    return self;
}

static PyMoxie_InternalJobScheduler*
PyMoxie_InternalJobScheduler_new
(
    PyTypeObject * type,
    PyObject     * Py_UNUSED(args),
    PyObject     * Py_UNUSED(kwargs)
)
{
    PyMoxie_InternalJobScheduler *self = NULL;

    if ((self = (PyMoxie_InternalJobScheduler*) type->tp_alloc(type, 0)) == NULL) {
        PyMoxie_LogErrorN("_moxie_core: tp_alloc failed for InternalJobScheduler.\n");
        return NULL;
    }
    self->state = NULL;
    return self;
}

static PyObject*
PyMoxie_MemoryMarker_repr
(
    PyMoxie_MemoryMarker *self
)
{
    uint8_t *addr = NULL;
    char      tag[MEM_TAG_BUFFER_SIZE];

    if (self->marker.chunk != NULL) {
        addr = self->marker.chunk->memory_start + self->marker.offset;
    }
    return PyUnicode_FromFormat("MemoryMarker(allocator=%S, tag=%s, version=%u, offset=%llu, address=%p)", self->allocator_name_str, mem_tag_to_ascii(tag, self->marker.tag), self->marker.version, self->marker.offset, addr);
}

static PyObject*
PyMoxie_MemoryAllocation_repr
(
    PyMoxie_MemoryAllocation *self
)
{
    char tag[MEM_TAG_BUFFER_SIZE];
    return PyUnicode_FromFormat("MemoryAllocation(address=%p, length=%zu, readonly=%S, source=%s(%S))", self->base_address, self->byte_length, self->readonly_bool, mem_tag_to_ascii(tag, self->tag), self->allocator_name_str);
}

static PyObject*
PyMoxie_MemoryMarker_str
(
    PyMoxie_MemoryMarker *self
)
{
    uint8_t *addr = NULL;
    char      tag[MEM_TAG_BUFFER_SIZE];

    if (self->marker.chunk != NULL) {
        addr = self->marker.chunk->memory_start + self->marker.offset;
    }
    return PyUnicode_FromFormat("%s %p [%llu] v%u (%S)", mem_tag_to_ascii(tag, self->marker.tag), addr, self->marker.offset, self->marker.version, self->allocator_name_str);
}

static PyObject*
PyMoxie_MemoryAllocation_str
(
    PyMoxie_MemoryAllocation *self
)
{
    char tag[MEM_TAG_BUFFER_SIZE];
    return PyUnicode_FromFormat("%s %p, %zu bytes (%S)", mem_tag_to_ascii(tag, self->tag), self->base_address, self->byte_length, self->allocator_name_str);
}

static int
PyMoxie_MemoryAllocation_getbuffer
(
    PyMoxie_MemoryAllocation *self,
    Py_buffer                *view,
    int                      flags
)
{
    int readonly;
    if (self->readonly_bool == Py_True) {
        readonly = 1;
    } else {
        readonly = 0;
    }
    return PyBuffer_FillInfo(view, (PyObject*) self, self->base_address, (Py_ssize_t) self->byte_length, readonly, flags);
}



static PyObject*
PyMoxie_Create_Memory_Allocator
(
    PyObject * Py_UNUSED(self),
    PyObject *           args ,
    PyObject *         kwargs
)
{
    PyMoxie_InternalAllocator *self_ = NULL;
    size_t                     guard = 0;
    char const              *nameval = NULL;
    char const               *tagval = NULL;
    Py_ssize_t                taglen = 0;
    Py_ssize_t               namelen = 0;
    Py_ssize_t            chunk_size = 0;
    Py_ssize_t             alignment = 0;
    uint32_t                   flags = MEM_ALLOCATION_FLAG_LOCAL | MEM_ALLOCATION_FLAG_HEAP | MEM_ALLOCATION_FLAG_GROWABLE;
    uint32_t                  access = MEM_ACCESS_FLAG_RDWR;
    mem_tag_t                    tag = 0;
    static char const      *kwlist[] ={"chunk_size","alignment","flags","access","name","tag",NULL};

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "nnIIz#z#:create_memory_allocator", (char**) kwlist, &chunk_size, &alignment, &flags, &access, &nameval, &namelen, &tagval, &taglen) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTupleAndKeywords(chunk_size,alignment,flags,access,name,tag) failed in PyMoxie_Create_Memory_Allocator.\n");
        return NULL;
    }
    if (chunk_size <= 0) {
        PyMoxie_LogErrorV("_moxie_core: chunk_size %zd supplied to PyMoxie_Create_Memory_Allocator expected to be > 0.\n", chunk_size);
        PyErr_SetString(PyExc_ValueError, "The chunk_size argument is expected to be greater than zero");
        return NULL;
    }
    if (alignment == 0) {
        alignment  = _MOXIE_CORE_DEFAULT_ALIGNMENT_BYTES;
    }
    if (alignment < 0) {
        PyMoxie_LogErrorV("_moxie_core: alignment %zd must be zero, or a positive power of two.\n", alignment);
        PyErr_SetString(PyExc_ValueError, "The alignment argument must be zero or a positive power of two");
        return NULL;
    }
    if ((alignment & (alignment - 1)) != 0) {
        PyMoxie_LogErrorV("_moxie_core: alignment %zd must be a non-zero power of two.\n", alignment);
        PyErr_SetString(PyExc_ValueError, "The alignment argument must specify a positive power of two, or zero");
        return NULL;
    }
    if (tagval != NULL && taglen != 4) {
        PyMoxie_LogErrorV("_moxie_core: tag value %.*s must have a length of 4 ASCII characters (%zd bytes supplied).\n", (int) taglen, tagval, taglen);
        PyErr_SetString(PyExc_ValueError, "The tag argument must be a string of exactly 4 ASCII characters");
        return NULL;
    }
    if (tagval != NULL) {
        tag = mem_tag(tagval[0], tagval[1], tagval[2], tagval[3]);
    }
    if (flags & MEM_ALLOCATION_FLAG_VIRTUAL) {
        guard = 1; /* Always include guard pages */
    }

    if ((self_ = PyMoxie_InternalAllocator_Create((size_t) chunk_size, guard, (size_t) alignment, flags, access, nameval, tag)) == NULL) {
        PyMoxie_LogErrorV("_moxie_core: Failed to create InternalAllocator with chunk_size %zu, alignment %zu, flags %08X, access %08X.\n", chunk_size, alignment, flags, access);
        return NULL;
    }
    return PyMoxie_Retain(self_);
}

static PyObject*
PyMoxie_Allocate_Memory
(
    PyObject * Py_UNUSED(self),
    PyObject *           args ,
    PyObject *         kwargs
)
{
    PyMoxie_InternalAllocator *self_ = NULL;
    PyMoxie_MemoryAllocation  *alloc = NULL;
    Py_ssize_t                nbytes = 0;
    Py_ssize_t                 align = _MOXIE_CORE_DEFAULT_ALIGNMENT_BYTES;
    void                       *addr = NULL;
    char const             *kwlist[] ={"arena","length","alignment",NULL};
    mem_marker_t                mark;

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "O!nn:allocate_memory", (char**) kwlist, &PyMoxie_InternalAllocatorType, &self_, &nbytes, &align) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTupleAndKeywords(arena,length,alignment) failed in PyMoxie_Allocate_Memory.\n");
        return NULL;
    }
    if (nbytes <= 0) {
        PyMoxie_LogErrorV("_moxie_core: Attempted to allocate %zd bytes from arena; size must be greater than zero.\n", nbytes);
        PyErr_SetString(PyExc_ValueError, "The length argument must be greater than zero");
        return NULL;
    }
    if (align == 0) {
        align  = _MOXIE_CORE_DEFAULT_ALIGNMENT_BYTES;
    }
    if (align <  0 || align > (Py_ssize_t) self_->allocator.page_size) {
        PyMoxie_LogErrorV("_moxie_core: Desired alignment %zd is outside of valid range [0, %u].\n", align, self_->allocator.page_size);
        PyErr_SetString(PyExc_ValueError, "The alignment argument is outside of the valid range");
        return NULL;
    }
    if ((align & (align - 1)) != 0) {
        PyMoxie_LogErrorV("_moxie_core: Desired alignment %zd must be a power of two integer value.\n", align);
        PyErr_SetString(PyExc_ValueError, "The alignment argument must be a power of two");
        return NULL;
    }
    if (self_->allocator.head == NULL) {
        PyMoxie_LogErrorV("_moxie_core: Attempted to allocate %zd bytes from disposed allocator.\n", nbytes);
        PyErr_SetString(PyExc_ValueError, "Attempted to allocate memory from a disposed allocator");
        return NULL;
    }

    mark = mem_allocator_mark(&self_->allocator);
    if ((addr = mem_allocator_alloc(&self_->allocator, (size_t) nbytes, (size_t) align)) == NULL) {
        PyMoxie_LogErrorV("_moxie_core: Memory allocation of %zd bytes with alignment %zd failed from arena %.*s.\n", nbytes, align, 4, (char*)(size_t) self_->allocator.allocator_tag);
        Py_RETURN_NONE; /* This is an expected possible outcome */
    }

    if ((alloc = PyMoxie_MemoryAllocation_Create(self_, addr, (size_t) nbytes, (size_t) align)) == NULL) {
        PyMoxie_LogErrorV("_moxie_core: Failed to allocate a new MemoryAllocation instance for %p, %zd bytes from arena %.*s.\n", addr, nbytes, 4, (char*)(size_t) self_->allocator.allocator_tag);
        mem_allocator_reset_to_marker(&self_->allocator, &mark);
        Py_RETURN_NONE; /* Not expected, but recoverable */
    }
    return PyMoxie_Retain(alloc);
}

static PyObject*
PyMoxie_Mark_Allocator
(
    PyObject * Py_UNUSED(self),
    PyObject *           args
)
{
    PyMoxie_InternalAllocator *self_ = NULL;
    PyMoxie_MemoryMarker     *marker = NULL;
    mem_marker_t                mark;

    if (PyArg_ParseTuple(args, "O!:create_allocator_marker", &PyMoxie_InternalAllocatorType, &self_) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTuple failed in PyMoxie_Mark_Allocator.\n");
        return NULL;
    }
    if (self_->allocator.head == NULL) {
        PyMoxie_LogErrorN("_moxie_core: Attempted to obtain a marker from a disposed allocator.\n");
        PyErr_SetString(PyExc_ValueError, "Attempted to obtain a marker from a disposed allocator");
        return NULL;
    }

    mark = mem_allocator_mark(&self_->allocator);
    if ((marker = PyMoxie_MemoryMarker_Create(self_, mark)) == NULL) {
        PyMoxie_LogErrorV("_moxie_core: Failed to allocate a new MemoryMarker instance for arena %.*s.\n", 4, (char*)(size_t) self_->allocator.allocator_tag);
        return NULL;
    }
    return PyMoxie_Retain(marker);
}

static PyObject*
PyMoxie_Reset_Allocator
(
    PyObject * Py_UNUSED(self),
    PyObject *           args
)
{
    PyMoxie_InternalAllocator *self_ = NULL;

    if (PyArg_ParseTuple(args, "O!:reset_memory_allocator", &PyMoxie_InternalAllocatorType, &self_) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTuple failed in PyMoxie_Reset_Allocator.\n");
        return NULL;
    }
    if (self_->allocator.head == NULL) {
        PyMoxie_LogErrorN("_moxie_core: Attempted to reset a disposed allocator.\n");
        PyErr_SetString(PyExc_ValueError, "Attempted to reset a disposed allocator");
        return NULL;
    }
    mem_allocator_reset(&self_->allocator);
    Py_RETURN_NONE;
}

static PyObject*
PyMoxie_Reset_Allocator_To_Marker
(
    PyObject * Py_UNUSED(self),
    PyObject *           args ,
    PyObject *         kwargs
)
{
    PyMoxie_InternalAllocator *self_ = NULL;
    PyMoxie_MemoryMarker     *marker = NULL;
    char const             *kwlist[] ={"arena","marker",NULL};

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!:reset_memory_allocator_to_marker", (char**) kwlist, &PyMoxie_InternalAllocatorType, &self_, &PyMoxie_MemoryMarkerType, &marker) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTupleAndKeywords(arena,marker) failed in PyMoxie_Reset_Allocator_To_Marker.\n");
        return NULL;
    }
    if (self_->allocator.head == NULL) {
        PyMoxie_LogErrorN("_moxie_core: Attempted to reset a disposed allocator to a previous marker.\n");
        PyErr_SetString(PyExc_ValueError, "Attempted to reset a disposed allocator to a previous marker");
        return NULL;
    }
    mem_allocator_reset_to_marker(&self_->allocator, &marker->marker);
    Py_RETURN_NONE;
}

static PyObject*
PyMoxie_Create_Job_Queue
(
    PyObject * Py_UNUSED(self),
    PyObject *           args ,
    PyObject *         kwargs
)
{
    PyMoxie_InternalJobQueue *self_ = NULL;
    uint32_t                     id = 0;
    static char const     *kwlist[] = {"id", NULL};

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "I:create_job_queue", (char**) kwlist, &id) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTupleAndKeywords(id) failed in PyMoxie_Create_Job_Queue");
        return NULL;
    }
    if ((self_ = PyMoxie_InternalJobQueue_Create(id)) == NULL) {
        PyMoxie_LogErrorV("_moxie_core: Failed to create job_queue_t instance with id=%u.\n", id);
        return NULL;
    }
    return PyMoxie_Retain(self_);
}

static PyObject*
PyMoxie_Flush_Job_Queue
(
    PyObject * Py_UNUSED(self),
    PyObject *           args
)
{
    PyMoxie_InternalJobQueue *self_ = NULL;

    if (PyArg_ParseTuple(args, "O!:flush_job_queue", &PyMoxie_InternalJobQueueType, &self_) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTuple failed in PyMoxie_Flush_Job_Queue.\n");
        return NULL;
    }
    if (self_->state == NULL) {
        PyMoxie_LogErrorN("_moxie_core: PyMoxie_InternalJobQueue::state field is NULL.\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobQueue state field is NULL");
        return NULL;
    }
    job_queue_flush(self_->state);
    Py_RETURN_NONE;
}

static PyObject*
PyMoxie_Signal_Job_Queue
(
    PyObject *   self,
    PyObject *   args,
    PyObject * kwargs
)
{
    PyMoxie_InternalJobQueue *self_ = NULL;
    uint32_t                 signal = JOB_QUEUE_SIGNAL_CLEAR;
    static char const     *kwlist[] ={"queue", "signal", NULL};

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "O!I:signal_job_queue", (char**) kwlist, &PyMoxie_InternalJobQueueType, &self_, &signal) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTupleAndKeywords(queue, signal) failed in PyMoxie_Signal_Job_Queue");
        return NULL;
    }
    if (self_->state == NULL) {
        PyMoxie_LogErrorN("_moxie_core: PyMoxie_InternalJobQueue::state field is NULL.\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobQueue state field is NULL");
        return NULL;
    }
    job_queue_signal(self_->state, signal);
    Py_RETURN_NONE;
}

static PyObject*
PyMoxie_Check_Job_Queue_Signal
(
    PyObject * Py_UNUSED(self),
    PyObject *           args
)
{
    PyMoxie_InternalJobQueue *self_ = NULL;
    uint32_t                 signal = JOB_QUEUE_SIGNAL_CLEAR;

    if (PyArg_ParseTuple(args, "O!:check_job_queue_signal", &PyMoxie_InternalJobQueueType, &self_) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTuple failed in PyMoxie_Check_Job_Queue_Signal.\n");
        return NULL;
    }
    if (self_->state == NULL) {
        PyMoxie_LogErrorN("_moxie_core: PyMoxie_InternalJobQueue::state field is NULL.\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobQueue state field is NULL");
        return NULL;
    }
    signal = job_queue_check_signal(self_->state);
    return PyLong_FromUnsignedLong((unsigned long) signal);
}

static PyObject*
PyMoxie_Acquire_JobContext
(
    PyObject * Py_UNUSED(self),
    PyObject *           args ,
    PyObject *         kwargs
)
{
    PyMoxie_InternalJobQueue     *queue = NULL;
    PyMoxie_InternalJobContext   *self_ = NULL;
    PyMoxie_InternalJobScheduler *sched = NULL;
    Py_ssize_t                      tid = 0;
    static char const         *kwlist[] ={"scheduler", "queue", "owner", NULL};

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!n:acquire_job_context", (char**) kwlist, &PyMoxie_InternalJobSchedulerType, &sched, &PyMoxie_InternalJobQueueType, &queue, &tid) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTupleAndKeywords(scheduler,queue,owner) failed in PyMoxie_Acquire_Job_Context.\n");
        return NULL;
    }
    if (sched->state == NULL) {
        PyMoxie_LogErrorN("_moxie_core: InternalJobScheduler passed to PyMoxie_Acquire_Job_Context has NULL state.\n");
        return NULL;
    }
    if (queue->state == NULL) {
        PyMoxie_LogErrorN("_moxie_core: InternalJobQueue passed to PyMoxie_Acquire_Job_Context has NULL state.\n");
        return NULL;
    }
    if ((self_ = PyMoxie_InternalJobContext_Create(queue, sched, (thread_id_t) tid)) == NULL) {
        PyMoxie_LogErrorV("_moxie_core: Failed to initialize InternalJobContext instance for thread 0x%zu.\n", (size_t) tid);
        return NULL;
    }
    return PyMoxie_Retain(self_);
}

static PyObject*
PyMoxie_Release_JobContext
(
    PyObject * Py_UNUSED(self),
    PyObject *           args
)
{
    PyMoxie_InternalJobContext *self_ = NULL;

    if (PyArg_ParseTuple(args, "O!:release_job_context", &PyMoxie_InternalJobContextType, &self_) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTuple failed in PyMoxie_Release_Job_Context.\n");
        return NULL;
    }
    if (self_->state == NULL) {
        PyMoxie_LogErrorN("_moxie_core: PyMoxie_InternalJobContext::state field is NULL.\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobContext state field is NULL");
        return NULL;
    }
    if (self_->sched == NULL) {
        PyMoxie_LogErrorN("_moxie_core: PyMoxie_InternalJobContext::sched field is NULL.\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobContext sched field is NULL");
        return NULL;
    }
    if (self_->queue == NULL) {
        PyMoxie_LogErrorN("_moxie_core: PyMoxie_InternalJobContext::queue field is NULL.\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobContext queue field is NULL");
        return NULL;
    }
    if (self_->sched->state == NULL) {
        PyMoxie_LogErrorN("_moxie_core: PyMoxie_InternalJobContext scheduler has NULL state field.\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobContext has invalid scheduler reference");
        return NULL;
    }
    job_scheduler_release_context(self_->sched->state, self_->state);
    Py_XDECREF(self_->sched); self_->sched = NULL;
    Py_XDECREF(self_->queue); self_->queue = NULL;
    self_->state = NULL;
    Py_RETURN_NONE;
}

static PyObject*
PyMoxie_Create_Python_Job
(
    PyObject * Py_UNUSED(self),
    PyObject *           args ,
    PyObject *         kwargs
)
{
    PyMoxie_InternalJobContext *self_ = NULL;
    PyMoxie_PythonJobState   *jobdata = NULL;
    job_descriptor_t         *jobdesc = NULL;
    PyObject               *id_to_ctx = NULL;
    PyObject                *callable = NULL;
    PyObject                *callargs = NULL;
    PyObject              *callkwargs = NULL;
    unsigned long           parent_id = JOB_ID_INVALID;
    static char const       *kwlist[] ={"context","id_to_ctx","parent","callable","args","kwargs",NULL};

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "O!OkOOO:create_python_job", (char**) kwlist, &PyMoxie_InternalJobContextType, &self_, &id_to_ctx, &parent_id, &callable, &callargs, &callkwargs) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTupleAndKeywords(context,id_to_ctx,parent,callable,args,kwargs) failed in PyMoxie_Create_Python_Job.\n");
        return NULL;
    }
#ifndef NDEBUG
    if (self_->state == NULL) {
        PyMoxie_LogErrorN("_moxie_core: InternalJobContext::state field is NULL.\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobContext state field is NULL");
        return NULL;
    }
    if (PyCallable_Check(callable) == 0) {
        PyMoxie_LogErrorN("_moxie_core: create_python_job received non-callable callable argument.\n");
        PyErr_SetString(PyExc_TypeError, "Value specified for callable argument should be a callable");
        return NULL;
    }
#endif
    if ((jobdesc = job_context_create_job(self_->state, sizeof(PyMoxie_PythonJobState), mem_align_of(PyMoxie_PythonJobState))) == NULL) {
        PyMoxie_LogErrorV("_moxie_core: Failed to allocate %zu bytes with alignment %zu for Python job state.\n", sizeof(PyMoxie_PythonJobState), mem_align_of(PyMoxie_PythonJobState));
        PyErr_SetString(PyExc_RuntimeError, "Failed to acquired storage for Python job");
        return NULL;
    }
    jobdesc->jobmain   = python_job_main;
    jobdesc->user1     =(uintptr_t) 0;
    jobdesc->user2     =(uintptr_t) 0;
    jobdesc->parent    = parent_id;
    jobdata = (PyMoxie_PythonJobState*) jobdesc->data;
    jobdata->id_to_ctx = PyMoxie_Retain(id_to_ctx);
    jobdata->callable  = PyMoxie_Retain(callable);
    jobdata->args      = PyMoxie_Retain(callargs);
    jobdata->kwargs    = PyMoxie_Retain(callkwargs);
    return PyLong_FromUnsignedLong((unsigned long) jobdesc->id);
}

static PyObject*
PyMoxie_Submit_Python_Job
(
    PyObject * Py_UNUSED(self),
    PyObject *           args ,
    PyObject *         kwargs
)
{
    PyMoxie_InternalJobContext *self_ = NULL;
    PyMoxie_InternalJobQueue   *queue = NULL;
    job_descriptor_t         *jobdesc = NULL;
    PyObject                 *deplist = NULL;
    long                  submit_type = JOB_SUBMIT_RUN;
    job_id_t                   job_id = JOB_ID_INVALID;
    job_id_t                   depjob = JOB_ID_INVALID;
    job_id_t                  depvals[16];
    Py_ssize_t const          MAXDEPS = mem_count_of(depvals);
    Py_ssize_t                numdeps = 0;
    Py_ssize_t                      i = 0;
    int                 submit_result = JOB_SUBMIT_SUCCESS;
    static char const       *kwlist[] ={"context","jobid","queue","depends","submit_type",NULL};

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "O!kOOl:submit_python_job", (char**) kwlist, &PyMoxie_InternalJobContextType, &self_, &job_id, &queue, &deplist, &submit_type) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTupleAndKeywords(context,jobid,queue,depends,submit_type) failed in PyMoxie_Submit_Python_Job.\n");
        return NULL;
    }
#ifndef NDEBUG
    if (self_->state == NULL) {
        PyMoxie_LogErrorN("_moxie_core: InternalJobContext::state field is NULL. Was job context released previously?\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobContext state field is NULL");
        return NULL;
    }
    if (self_->sched == NULL) {
        PyMoxie_LogErrorN("_moxie_core: InternalJobContext::sched field is NULL.\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobContext sched field is NULL");
        return NULL;
    }
    if (self_->queue == NULL) {
        PyMoxie_LogErrorN("_moxie_core: InternalJobContext::queue field is NULL.\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobContext queue field is NULL");
        return NULL;
    }
    if (self_->sched->state == NULL) {
        PyMoxie_LogErrorN("_moxie_core: InternalJobContext::sched field has NULL state field.\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobContext sched field has NULL state field");
        return NULL;
    }
    if (submit_type != JOB_SUBMIT_RUN && submit_type != JOB_SUBMIT_CANCEL) {
        PyMoxie_LogErrorV("_moxie_core: Invalid submit_type %u supplied to submit_python_job.\n", submit_type);
        PyErr_SetString(PyExc_ValueError, "Invalid submit_type supplied to submit_python_job");
        return NULL;
    }
#endif
    if ((jobdesc = job_scheduler_resolve_job_id(self_->sched->state, job_id)) == NULL) {
        return PyLong_FromLong((long) JOB_SUBMIT_INVALID_JOB);
    }
    if (queue == NULL || queue == (PyMoxie_InternalJobQueue*) Py_None) {
        /* Use the default queue for the job context. */
        jobdesc->target = NULL;
    } else {
#ifndef NDEBUG
        if (Py_TYPE(queue) != &PyMoxie_InternalJobQueueType) {
            PyMoxie_LogErrorN("_moxie_core: Expected InternalJobQueue instance for queue argument in submit_python_job.\n");
            PyErr_SetString(PyExc_TypeError, "Expected InternalJobQueue instance for queue argument");
            return NULL;
        }
#endif
        /* Submit the job to the supplied queue. */
        jobdesc->target = queue->state;
    }
    if (deplist != NULL && deplist != Py_None) {
#ifndef NDEBUG
        if (PyList_Check(deplist) == 0) {
            PyMoxie_LogErrorN("_moxie_core: Expected List[int] for depends argument.\n");
            PyErr_SetString(PyExc_TypeError, "Expected List[int] for depends argument");
            return NULL;
        }
#endif
        if ((numdeps = PyList_Size(deplist)) > MAXDEPS) {
            PyMoxie_LogErrorV("_moxie_core: Rejecting job 0x%08X; dependency count %zu exceeds maximum %zu.\n", job_id, (size_t) numdeps, (size_t) MAXDEPS);
            PyErr_SetString(PyExc_RuntimeError, "Rejecting job submission; job has too many dependencies");
            return NULL;
        }
        for (i = 0; i < numdeps; ++i) {
            PyObject *val = PyList_GetItem(deplist, i);
#ifndef NDEBUG
            if (PyLong_Check(val) == 0) {
                PyMoxie_LogErrorV("_moxie_core: Job 0x%08X dependency %zu does not have expected int type.\n", job_id, (size_t) i);
                PyErr_SetString(PyExc_TypeError, "Job dependency list contains non-int values");
                return NULL;
            }
#endif
            if ((depjob = (job_id_t) PyLong_AsUnsignedLong(val)) != JOB_ID_INVALID) {
                depvals[i] = depjob;
            } else {
                PyMoxie_LogErrorV("_moxie_core: Job 0x%08X dependency %zu is JOB_ID_INVALID; ignoring.\n", job_id, (size_t) i);
                continue;
            }
        }
    }
    submit_result = job_context_submit_job(self_->state, jobdesc, depvals, (size_t) numdeps, (int) submit_type);
    return PyLong_FromLong((long) submit_result);
}

static PyObject*
PyMoxie_Cancel_Job
(
    PyObject * Py_UNUSED(self),
    PyObject *           args ,
    PyObject *         kwargs
)
{
    PyMoxie_InternalJobContext *self_ = NULL;
    job_id_t                   job_id = JOB_ID_INVALID;
    int                     job_state = JOB_STATE_UNINITIALIZED;
    static char const       *kwlist[] ={"context","jobid",NULL};

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "O!k:cancel_job", (char**) kwlist, &PyMoxie_InternalJobContextType, &self_, &job_id) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTupleAndKeywords(context,jobid) failed in PyMoxie_Cancel_Job.\n");
        return NULL;
    }
    if (self_->state == NULL) {
        PyMoxie_LogErrorN("_moxie_core: InternalJobContext::state field is NULL. Was job context released previously?\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobContext state field is NULL");
        return NULL;
    }
    job_state = job_context_cancel_job(self_->state, job_id);
    return PyLong_FromLong((long)  job_state);
}

static PyObject*
PyMoxie_Complete_Job
(
    PyObject * Py_UNUSED(self),
    PyObject *           args ,
    PyObject *         kwargs
)
{
    PyMoxie_InternalJobContext *self_ = NULL;
    job_descriptor_t         *jobdesc = NULL;
    job_id_t                   job_id = JOB_ID_INVALID;
    static char const       *kwlist[] ={"context","jobid",NULL};

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "O!k:cancel_job", (char**) kwlist, &PyMoxie_InternalJobContextType, &self_, &job_id) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTupleAndKeywords(context,jobid) failed in PyMoxie_Complete_Job.\n");
        return NULL;
    }
    if (self_->state == NULL) {
        PyMoxie_LogErrorN("_moxie_core: InternalJobContext::state field is NULL. Was job context released previously?\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobContext state field is NULL");
        return NULL;
    }
    if (self_->sched == NULL) {
        PyMoxie_LogErrorN("_moxie_core: InternalJobContext::sched field is NULL.\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobContext sched field is NULL");
        return NULL;
    }
    if (self_->queue == NULL) {
        PyMoxie_LogErrorN("_moxie_core: InternalJobContext::queue field is NULL.\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobContext queue field is NULL");
        return NULL;
    }
    if (self_->sched->state == NULL) {
        PyMoxie_LogErrorN("_moxie_core: InternalJobContext::sched field has NULL state field.\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobContext sched field has NULL state field");
        return NULL;
    }
    if ((jobdesc = job_scheduler_resolve_job_id(self_->sched->state, job_id)) == NULL) {
        PyMoxie_LogErrorV("_moxie_core: Failed to resolve job descriptor for job 0x%08X; job cannot be completed.\n", job_id);
        Py_RETURN_NONE;
    }
    job_context_complete_job(self_->state, jobdesc);
    Py_RETURN_NONE;
}

static PyObject*
PyMoxie_Wait_For_Job
(
    PyObject * Py_UNUSED(self),
    PyObject *           args ,
    PyObject *         kwargs
)
{
    PyMoxie_InternalJobContext *self_ = NULL;
    job_id_t                   job_id = JOB_ID_INVALID;
    int                   wait_result = 0;
    static char const       *kwlist[] ={"context","jobid",NULL};

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "O!k:cancel_job", (char**) kwlist, &PyMoxie_InternalJobContextType, &self_, &job_id) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTupleAndKeywords(context,jobid) failed in PyMoxie_Wait_For_Job.\n");
        return NULL;
    }
    if (self_->state == NULL) {
        PyMoxie_LogErrorN("_moxie_core: InternalJobContext::state field is NULL. Was job context released previously?\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobContext state field is NULL");
        return NULL;
    }
    Py_BEGIN_ALLOW_THREADS
        wait_result = job_context_wait_job(self_->state, job_id);
    Py_END_ALLOW_THREADS
    return PyLong_FromLong((long) wait_result);
}

static PyObject*
PyMoxie_Run_Next_Job
(
    PyObject * Py_UNUSED(self),
    PyObject *           args
)
{
    PyMoxie_InternalJobContext *self_ = NULL;
    job_descriptor_t         *jobdesc = NULL;
    job_id_t                   job_id = JOB_ID_INVALID;

    if (PyArg_ParseTuple(args, "O!:run_next_job", &PyMoxie_InternalJobContextType, &self_) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTuple failed in PyMoxie_Run_Next_Job.\n");
        return NULL;
    }
    if (self_->state == NULL) {
        PyMoxie_LogErrorN("_moxie_core: PyMoxie_InternalJobContext::state field is NULL.\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobContext state field is NULL");
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
        if ((jobdesc = job_context_wait_ready_job(self_->state)) != NULL) {
            job_id        = jobdesc->id;
            jobdesc->exit = jobdesc->jobmain(self_->state, jobdesc, JOB_CALL_TYPE_EXECUTE);
            job_context_complete_job(self_->state, jobdesc);
        }
    Py_END_ALLOW_THREADS
    return PyLong_FromUnsignedLong((unsigned long) job_id);
}

static PyObject*
PyMoxie_Run_Next_Job_No_Completion
(
    PyObject * Py_UNUSED(self),
    PyObject *           args
)
{
    PyMoxie_InternalJobContext *self_ = NULL;
    job_descriptor_t         *jobdesc = NULL;
    job_id_t                   job_id = JOB_ID_INVALID;

    if (PyArg_ParseTuple(args, "O!:run_next_jobi_no_completion", &PyMoxie_InternalJobContextType, &self_) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTuple failed in PyMoxie_Run_Next_Job_No_Completion.\n");
        return NULL;
    }
    if (self_->state == NULL) {
        PyMoxie_LogErrorN("_moxie_core: PyMoxie_InternalJobContext::state field is NULL.\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobContext state field is NULL");
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
        if ((jobdesc = job_context_wait_ready_job(self_->state)) != NULL) {
            job_id        = jobdesc->id;
            jobdesc->exit = jobdesc->jobmain(self_->state, jobdesc, JOB_CALL_TYPE_EXECUTE);
        }
    Py_END_ALLOW_THREADS
    return PyLong_FromUnsignedLong((unsigned long) job_id);
}

static PyObject*
PyMoxie_Create_Job_Scheduler
(
    PyObject * Py_UNUSED(self),
    PyObject *           args ,
    PyObject *         kwargs
)
{
    PyMoxie_InternalJobScheduler *self_ = NULL;
    uint32_t                  ncontexts = 1;
    static char const         *kwlist[] ={"context_count",NULL};

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "|I:create_job_scheduler", (char**) kwlist, &ncontexts) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTuple(context_count) failed in PyMoxie_Create_Job_Scheduler.\n");
        return NULL;
    }
    if ((self_ = PyMoxie_InternalJobScheduler_Create(ncontexts)) == NULL) {
        PyMoxie_LogErrorV("_moxie_core: Failed to allocate job scheduler with %u context(s).\n", ncontexts);
        return NULL;
    }
    return PyMoxie_Retain(self_);
}

static PyObject*
PyMoxie_Terminate_Job_Scheduler
(
    PyObject * Py_UNUSED(self),
    PyObject *           args
)
{
    PyMoxie_InternalJobScheduler *self_ = NULL;

    if (PyArg_ParseTuple(args, "O!:terminate_job_scheduler", &PyMoxie_InternalJobSchedulerType, &self_) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTuple failed in PyMoxie_Terminate_Job_Scheduler.\n");
        return NULL;
    }
    if (self_->state == NULL) {
        PyMoxie_LogErrorN("_moxie_core: PyMoxie_InternalJobScheduler::state field is NULL.\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobScheduler state field is NULL");
        return NULL;
    }
    job_scheduler_terminate(self_->state);
    Py_RETURN_NONE;
}

static PyObject*
PyMoxie_Get_Job_Queue_Worker_Count
(
    PyObject * Py_UNUSED(self),
    PyObject *           args ,
    PyObject *         kwargs
)
{
    PyMoxie_InternalJobScheduler *self_ = NULL;
    uint32_t                   queue_id = 0;
    uint32_t                   nworkers = 0;
    static char const         *kwlist[] ={"scheduler","id",NULL};

    if (PyArg_ParseTupleAndKeywords(args, kwargs, "O!I:get_worker_count_for_queue", (char**) kwlist, &PyMoxie_InternalJobSchedulerType, &self_, &queue_id) == 0) {
        PyMoxie_LogErrorN("_moxie_core: PyArg_ParseTupleAndKeywords(scheduler,id) failed in PyMoxie_Get_Job_Queue_Worker_Count.\n");
        return NULL;
    }
    if (self_->state == NULL) {
        PyMoxie_LogErrorN("_moxie_core: InternalJobScheduler::state field is NULL.\n");
        PyErr_SetString(PyExc_ValueError, "InternalJobScheduler state field is NULL");
        return NULL;
    }
    nworkers = job_scheduler_get_queue_worker_count(self_->state, queue_id);
    return PyLong_FromUnsignedLong((unsigned long)  nworkers);
}

#ifdef __cplusplus
extern "C" {
#endif

PyMoxie_MemoryMarker*
PyMoxie_MemoryMarker_Create
(
    PyMoxie_InternalAllocator *arena,
    struct mem_marker_t         mark
)
{
    PyMoxie_MemoryMarker *inst = NULL;

    if ((inst = (PyMoxie_MemoryMarker*) PyMoxie_MemoryMarkerType.tp_alloc(&PyMoxie_MemoryMarkerType, 0)) == NULL) {
        PyMoxie_LogErrorN("_moxie_core: Failed to allocate memory for a new MemoryMarker instance.\n");
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate a new MemoryMarker");
        goto cleanup_and_fail;
    }
    if (arena != NULL) {
        inst->allocator_name_str = PyMoxie_Retain(arena->allocator_name_str);
        inst->allocator_tag_int  = PyMoxie_Retain(arena->allocator_tag_int);
    } else {
        inst->allocator_name_str = PyMoxie_Retain(Py_None);
        inst->allocator_tag_int  = PyMoxie_Retain(Py_None);
    }
    inst->marker = mark;
    return inst;

cleanup_and_fail:
    if (inst != NULL) {
        PyMoxie_MemoryMarkerType.tp_free(inst);
    } return NULL;
}

PyMoxie_MemoryAllocation*
PyMoxie_MemoryAllocation_Create
(
    PyMoxie_InternalAllocator *arena,
    void                    *address,
    size_t                    length,
    size_t                 alignment
)
{
    PyMoxie_MemoryAllocation *inst = NULL;
    PyObject     *base_address_int = NULL;
    PyObject           *length_int = NULL;
    PyObject        *alignment_int = NULL;
    PyObject        *readonly_bool = NULL;

    if (address  == NULL) {
        alignment = 1;
        length    = 0;
    }
    if ((base_address_int = PyLong_FromVoidPtr(address)) == NULL) {
        goto cleanup_and_fail;
    }
    if ((alignment_int = PyLong_FromSize_t(alignment)) == NULL) {
        goto cleanup_and_fail;
    }
    if ((length_int = PyLong_FromSize_t(length)) == NULL) {
        goto cleanup_and_fail;
    }
    if ((inst = (PyMoxie_MemoryAllocation*) PyMoxie_MemoryAllocationType.tp_alloc(&PyMoxie_MemoryAllocationType, 0)) == NULL) {
        goto cleanup_and_fail;
    }
    if (arena != NULL) {
        inst->allocator_name_str = PyMoxie_Retain(arena->allocator_name_str);
        inst->allocator_tag_int  = PyMoxie_Retain(arena->allocator_tag_int);
        if ((arena->allocator.access_flags & MEM_ACCESS_FLAG_WRITE) != 0) {
            readonly_bool = PyMoxie_Retain(Py_False);
        } else {
            readonly_bool = PyMoxie_Retain(Py_True);
        }
    } else {
        inst->allocator_name_str = PyMoxie_Retain(Py_None);
        inst->allocator_tag_int  = PyMoxie_Retain(Py_None);
        if (address != NULL && length != 0) {
            readonly_bool = PyMoxie_Retain(Py_False);
        } else {
            readonly_bool = PyMoxie_Retain(Py_True);
        }
    }
    return inst;

cleanup_and_fail:
    PyMoxie_LogErrorN("_moxie_core: Failed to allocate memory for a MemoryAllocation object.\n");
    PyErr_SetString(PyExc_MemoryError, "Failed to allocate a new MemoryAllocation");
    if (inst != NULL) {
        PyMoxie_MemoryAllocationType.tp_free(inst);
    }
    Py_DECREF(readonly_bool);
    Py_DECREF(alignment_int);
    Py_DECREF(length_int);
    Py_DECREF(base_address_int);
    return NULL;
}

PyMoxie_InternalAllocator*
PyMoxie_InternalAllocator_Create
(
    size_t chunk_size,
    size_t guard_size,
    size_t  alignment,
    uint32_t    flags,
    uint32_t   access,
    char const  *name,
    mem_tag_t     tag
)
{
    PyMoxie_InternalAllocator *inst = NULL;
    PyObject              *name_str = NULL;
    PyObject               *tag_int = NULL;
    PyObject         *page_size_int = NULL;
    PyObject         *growable_bool = NULL;
    mem_allocator_t      *alloc_ptr = NULL;
    mem_allocator_t           alloc;

    if (alignment == 0) {
        alignment  = _MOXIE_CORE_DEFAULT_ALIGNMENT_BYTES;
    }

    mem_zero(&alloc, sizeof(alloc));
    if ((alloc_ptr = mem_allocator_create(&alloc, chunk_size, guard_size, alignment, flags, access, name, tag)) == NULL) {
        goto cleanup_and_fail;
    }
    if (alloc.allocator_flags & MEM_ALLOCATION_FLAG_GROWABLE) {
        growable_bool = PyMoxie_Retain(Py_True);
    } else {
        growable_bool = PyMoxie_Retain(Py_False);
    }
    if ((page_size_int = PyLong_FromUnsignedLong((unsigned long) alloc.page_size)) == NULL) {
        goto cleanup_and_fail;
    }
    if ((tag_int = PyLong_FromUnsignedLong((unsigned long) alloc.allocator_tag)) == NULL) {
        goto cleanup_and_fail;
    }
    if (name != NULL) {
        if ((name_str = PyUnicode_FromString(name)) == NULL) {
            goto cleanup_and_fail;
        }
    } else {
        name_str = PyMoxie_Retain(Py_None);
    }
    if ((inst = (PyMoxie_InternalAllocator*) PyMoxie_InternalAllocatorType.tp_alloc(&PyMoxie_InternalAllocatorType, 0)) == NULL) {
        goto cleanup_and_fail;
    }
    inst->allocator_name_str = name_str;
    inst->allocator_tag_int  = tag_int;
    inst->page_size_int      = page_size_int;
    inst->growable_bool      = growable_bool;
    mem_copy(&inst->allocator, &alloc, sizeof(mem_allocator_t));
    return inst;

cleanup_and_fail:
    PyMoxie_LogErrorV("_moxie_core: Failed to allocate memory for an InternalAllocator object %s with chunk size %zu.\n", name, chunk_size);
    PyErr_SetString(PyExc_MemoryError, "Failed to allocate a new InternalAllocator");
    if (alloc_ptr != NULL) {
        mem_allocator_delete(alloc_ptr);
    }
    if (inst != NULL) {
        PyMoxie_InternalAllocatorType.tp_free(inst);
    }
    Py_DECREF(growable_bool);
    Py_DECREF(page_size_int);
    Py_DECREF(tag_int);
    Py_DECREF(name_str);
    return NULL;
}

PyMoxie_InternalJobQueue*
PyMoxie_InternalJobQueue_Create
(
    uint32_t queue_id
)
{
    PyMoxie_InternalJobQueue *inst = NULL;
    struct job_queue_t      *state = NULL;

    if ((state = job_queue_create(queue_id)) == NULL) {
        PyMoxie_LogErrorN("_moxie_core: Failed to allocate job_queue_t in PyMoxie_InternalJobQueue_Create.\n");
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate a new waitable job queue");
        goto cleanup_and_fail;
    }
    if ((inst  =(PyMoxie_InternalJobQueue*) PyMoxie_InternalJobQueue_new(&PyMoxie_InternalJobQueueType, NULL, NULL)) == NULL) {
        PyMoxie_LogErrorN("_moxie_core: Failed to allocate memory for a new InternalJobQueue instance.\n");
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate a new InternalJobQueue");
        goto cleanup_and_fail;
    }
    inst->state = state;
    return inst;

cleanup_and_fail:
    if (inst != NULL) {
        PyMoxie_InternalJobQueueType.tp_free(inst);
    }
    if (state != NULL) {
        job_queue_delete(state);
    } return NULL;
}

PyMoxie_InternalJobContext*
PyMoxie_InternalJobContext_Create
(
    PyMoxie_InternalJobQueue         *queue,
    PyMoxie_InternalJobScheduler *scheduler,
    thread_id_t                   thread_id
)
{
    PyMoxie_InternalJobContext *inst = NULL;
    struct job_context_t      *state = NULL;

    if ((state = job_scheduler_acquire_context(scheduler->state, queue->state, thread_id)) == NULL) {
        PyMoxie_LogErrorV("_moxie_core: Failed to acquire job_context_t bound to thread %ld in PyMoxie_InternalJobContext_Create.\n", thread_id);
        PyErr_SetString(PyExc_MemoryError, "Failed to acquire a new job context");
        goto cleanup_and_fail;
    }
    if ((inst  =(PyMoxie_InternalJobContext*) PyMoxie_InternalJobContext_new(&PyMoxie_InternalJobContextType, NULL, NULL)) == NULL) {
        PyMoxie_LogErrorV("_moxie_core: Failed to allocate memory for a new InternalJobContext instance bound to thread %ld.\n", thread_id);
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate a new InternalJobContext");
        goto cleanup_and_fail;
    }
    inst->state = state;
    inst->queue =(PyMoxie_InternalJobQueue    *) PyMoxie_Retain(queue);
    inst->sched =(PyMoxie_InternalJobScheduler*) PyMoxie_Retain(scheduler);
    return inst;

cleanup_and_fail:
    if (inst != NULL) {
        PyMoxie_InternalJobSchedulerType.tp_free(inst);
    }
    if (state != NULL) {
        job_scheduler_release_context(scheduler->state, state);
    } return NULL;
}

PyMoxie_InternalJobScheduler*
PyMoxie_InternalJobScheduler_Create
(
    uint32_t context_count
)
{
    PyMoxie_InternalJobScheduler *inst = NULL;
    struct job_scheduler_t      *state = NULL;

    if ((state = job_scheduler_create((size_t) context_count)) == NULL) {
        PyMoxie_LogErrorN("_moxie_core: Failed to allocate job_scheduler_t in PyMoxie_InternalJobScheduler_Create.\n");
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate a new job scheduler");
        goto cleanup_and_fail;
    }
    if ((inst  =(PyMoxie_InternalJobScheduler*) PyMoxie_InternalJobScheduler_new(&PyMoxie_InternalJobSchedulerType, NULL, NULL)) == NULL) {
        PyMoxie_LogErrorN("_moxie_core: Failed to allocate memory for a new InternalJobScheduler instance.\n");
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate a new InternalJobScheduler");
        goto cleanup_and_fail;
    }
    inst->state = state;
    return inst;

cleanup_and_fail:
    if (inst != NULL) {
        PyMoxie_InternalJobSchedulerType.tp_free(inst);
    }
    if (state != NULL) {
        job_scheduler_delete(state);
    } return NULL;
}


PyMODINIT_FUNC
PyInit__moxie_core
(
    void
)
{
    PyObject *_moxie_core = NULL;

    if ((_moxie_core = PyModule_Create(&PyMoxie_Core_ModuleDefinition)) == NULL) {
        return NULL;
    }
    if (extension_ready_types(_moxie_core) == -1) {
        return NULL;
    }
    if (extension_register_constants(_moxie_core) == -1) {
        return NULL;
    }

    return _moxie_core;
}

#ifdef __cplusplus
}; /* extern "C" */
#endif
