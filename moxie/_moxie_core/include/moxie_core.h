/**
 * moxie_core.h: Define the Python type wrappers for the types exposed by the extension.
 * The design philosophy here is that these types should do the minimum amount of work necessary - they basically just carry around pointers to the corresponding native types.
 * The memory management types break this pattern somewhat, and expose some extra functionality for debugging purposes.
 * All of the higher-level logic should be implemented in Python.
 */
#ifndef __MOXIE__MOXIE_CORE_MOXIE_CORE_H__
#define __MOXIE__MOXIE_CORE_MOXIE_CORE_H__

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef  PY_SSIZE_T_CLEAN
#define  PY_SSIZE_T_CLEAN
#endif
#include <Python.h>
#include <structmember.h>


#include "internal/platform.h"
#include "internal/memory.h"
#include "internal/scheduler.h"


#ifndef _MOXIE_CORE_HELPER_MACROS
#define _MOXIE_CORE_HELPER_MACROS
/**
 * Retain a reference to a Python object and return the object cast to PyObject.
 */
#define PyMoxie_Retain(_obj_instance)                                          \
    (Py_INCREF(_obj_instance), (PyObject*)_obj_instance)

/**
 * Check if a Python object is not an instance of a given type.
 */
#define PyMoxie_NotType(_obj_instance, _type_class)                            \
    ((_obj_instance) == NULL || (_obj_instance) == Py_None || Py_TYPE(_obj_instance) != &_type_class)
#endif

/**
 * The default alignment for memory allocations, in bytes.
 * The default alignment should be suitable for SIMD instructions.
 */
#ifndef _MOXIE_CORE_DEFAULT_ALIGNMENT_BYTES
#define _MOXIE_CORE_DEFAULT_ALIGNMENT_BYTES                                    16
#endif

extern PyTypeObject PyMoxie_MemoryMarkerType;                                  /* Python type MemoryMarker         => PyMoxie_MemoryMarker.         */
extern PyTypeObject PyMoxie_MemoryAllocationType;                              /* Python type MemoryAllocation     => PyMoxie_MemoryAllocation.     */
extern PyTypeObject PyMoxie_InternalAllocatorType;                             /* Python type InternalAllocator    => PyMoxie_InternalAllocator.    */
extern PyTypeObject PyMoxie_InternalJobQueueType;                              /* Python type InternalJobQueue     => PyMoxie_InternalJobQueue.     */
extern PyTypeObject PyMoxie_InternalJobContextType;                            /* Python type InternalJobContext   => PyMoxie_InternalJobContext.   */
extern PyTypeObject PyMoxie_InternalJobSchedulerType;                          /* Python type InternalJobScheduler => PyMoxie_InternalJobScheduler. */

typedef struct PyMoxie_MemoryMarker {                                          /* Python type wrapper for moxie's mem_marker_t struct. */
    PyObject_HEAD
    PyObject               *allocator_name_str;                                /* The name of the InternalAllocator from which the marker was retrieved. */
    PyObject                *allocator_tag_int;                                /* The four character code tag of the InternalAllocator from which the marker was obtained. */
    mem_marker_t                        marker;                                /* The internal marker data. */
} PyMoxie_MemoryMarker;

typedef struct PyMoxie_InternalAllocator {                                     /* Python type wrapper for moxie's mem_allocator_t struct. */
    PyObject_HEAD
    PyObject               *allocator_name_str;                                /* The name of the InternalAllocator, used for debugging. */
    PyObject                *allocator_tag_int;                                /* The four character code tag of the InternalAllocator, used for debugging. */
    PyObject                    *page_size_int;                                /* The operating system page size, in bytes. */
    PyObject                    *growable_bool;                                /* Specifies whether or not the memory allocator can grow. */
    mem_allocator_t                  allocator;                                /* The internal allocator data. */
} PyMoxie_InternalAllocator;

typedef struct PyMoxie_MemoryAllocation {                                      /* Python type wrapper for a memory allocation. */
    PyObject_HEAD
    PyObject                 *base_address_int;                                /* The address of the first byte of the allocation, or 0 if the allocation is invalid. */
    PyObject                       *length_int;                                /* The total number of bytes allocated. */
    PyObject                    *alignment_int;                                /* The requested alignment of the base address, in bytes. */
    PyObject               *allocator_name_str;                                /* The name of the InternalAllocator from which the memory was allocated. */
    PyObject                *allocator_tag_int;                                /* The four character code tag of the InternalAllocator from which the memory was allocated. */
    PyObject                    *readonly_bool;                                /* Specifies whether or not the memory is read-only. */
    void                         *base_address;                                /* The native address of the first byte of the memory allocation, or NULL if the allocation is invalid. */
    size_t                         byte_length;                                /* The total number of bytes allocated. */
    mem_tag_t                              tag;                                /* A copy of the allocator's four character code tag, used for debugging (allowing identification of the struct in a memory dump). */
} PyMoxie_MemoryAllocation;

typedef struct PyMoxie_InternalJobQueue {                                      /* Python type wrapper for moxie's job_queue_t struct, providing a waitable job queue. */
    PyObject_HEAD
    struct job_queue_t                  *state;                                /* The native job_queue_t instance, allocated on the process heap. Job queues can be created external to the scheduler. */
} PyMoxie_InternalJobQueue;

typedef struct PyMoxie_InternalJobContext {                                    /* Python type wrapper for moxie's job_context_t struct, holding the state required to create, submit, execute and wait on jobs. */
    PyObject_HEAD
    struct job_context_t                *state;                                /* The native job_context_t instance, allocated from job scheduler memory. The context is destroyed when the scheduler is destroyed. */
    struct PyMoxie_InternalJobQueue     *queue;                                /* The retained reference to the default queue to which jobs are submitted, and on which the thread owning the context waits for work. */
    struct PyMoxie_InternalJobScheduler *sched;                                /* The retained reference to the scheduler instance from which the context was acquired. */
} PyMoxie_InternalJobContext;

typedef struct PyMoxie_InternalJobScheduler {                                  /* Python type wrapper for moxie's job_scheduler_t type, used to acquire and release job contexts. */
    PyObject_HEAD
    struct job_scheduler_t              *state;                                /* The native job_scheduler_t instance, allocated from process virtual address space. */
} PyMoxie_InternalJobScheduler;

typedef struct PyMoxie_PythonJobState {                                        /* Internal job state data storing retained references for a job defined in Python code. */
    PyObject                         *callable;                                /* The Python callable object representing the callback. */
    PyObject                             *args;                                /* The tuple of positional arguments to supply to the callback. */
    PyObject                           *kwargs;                                /* The dictionary of keyword arguments to supply to the callback. */
    PyObject                        *id_to_ctx;                                /* The dictionary used to map thread identifiers to Python JobContext instances. */
} PyMoxie_PythonJobState;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Allocate and initialize a new memory arena marker.
 * @param arena The arena from which the marker was obtained.
 * @param mark The marker representing the state of the memory arena.
 * @return The new instance of the memory arena marker, or NULL if resource allocation failed.
 * The caller is responsible for incrementing the reference count of the returned object.
 */
extern PyMoxie_MemoryMarker*
PyMoxie_MemoryMarker_Create
(
    PyMoxie_InternalAllocator *arena,
    mem_marker_t                mark
);

/**
 * Allocate and initialize a new memory allocation descriptor.
 * @param arena The arena from which the allocated memory block was acquired.
 * @param address The base address of the memory block.
 * @param length The length of the memory block, in bytes.
 * @return The new instance of the memory allocation descriptor, or NULL if resource allocation failed.
 * The caller is responsible for incrementing the reference count of the returned object.
 */
extern PyMoxie_MemoryAllocation*
PyMoxie_MemoryAllocation_Create
(
    PyMoxie_InternalAllocator *arena,
    void                    *address,
    size_t                    length,
    size_t                 alignment
);

/**
 * Allocate and initialize a new arena memory allocator.
 * @param chunk_size The size of the initial memory allocation, in bytes.
 * @param guard_size The size of the guard region, in bytes. This value may be zero.
 * @param alignment The desired alignment of the allocated region, in bytes. This must be a non-zero power of two.
 * @param flags A combination of one or more bitwise OR'd values from the mem_allocation_flags_e enumeration.
 * @param access A combination of one or more bitwise OR'd values from the mem_access_flags_e enumeration.
 * @param name A nul-terminated, UTF-8 encoded string specifying a name for the allocator.
 * @param tag A four byte value used to identify the allocator in memory.
 * @return The new instance of the arena allocator, or NULL if resource allocation failed.
 * The caller is responsible for incrementing the reference count of the returned object.
 */
extern PyMoxie_InternalAllocator*
PyMoxie_InternalAllocator_Create
(
    size_t chunk_size,
    size_t guard_size,
    size_t  alignment,
    uint32_t    flags,
    uint32_t   access,
    char const  *name,
    mem_tag_t     tag
);

/**
 * Allocate and initialize to empty a new fixed-length, waitable queue for storing ready-to-run jobs.
 * The queue capacity is always JOB_COUNT_MAX and the elements stored are always pointers to job_descriptor_t.
 * @param queue_id An application-defined identifier for the job queue.
 * @return The new instance of an empty queue, or NULL if resource allocation failed.
 * The caller is responsible for incrementing the reference count of the returned object.
 */
extern PyMoxie_InternalJobQueue*
PyMoxie_InternalJobQueue_Create
(
    uint32_t queue_id
);

/**
 * Allocate and initialize a new job context that can be used to create, submit, execute and wait for jobs.
 * @param queue The job queue on which the context will wait for jobs, and the default queue to which jobs are submitted.
 * @param scheduler The job scheduler from which the context should be acquired.
 * @param thread_id The identifier of the thread that will own the context.
 * @return The new instance of the internal job context, or NULL if resource allocation failed or another error occurred.
 * The caller is responsible for incrementing the reference count of the returned object.
 */
extern PyMoxie_InternalJobContext*
PyMoxie_InternalJobContext_Create
(
    PyMoxie_InternalJobQueue         *queue,
    PyMoxie_InternalJobScheduler *scheduler,
    thread_id_t                   thread_id
);

/**
 * Allocate and initialize a new job scheduler.
 * @param context_count The number of job_context_t expected to be required by the application (typically one per-thread that interacts with the job system).
 * @return The new instance of the internal job scheduler, or NULL if resource allocation failed or another error occurred.
 * The caller is responsible for incrementing the reference count of the returned object.
 */
extern PyMoxie_InternalJobScheduler*
PyMoxie_InternalJobScheduler_Create
(
    uint32_t context_count
);

/**
 * Given a job identifier, retrieve the associated job_descriptor_t instance.
 * The caller is responsible for validating the scheduler instance.
 * @param scheduler The job scheduler defining the namespace for the job identifier.
 * @param job_id The identifier of the job.
 * @return A pointer to the job descriptor structure, or NULL if the job ID is invalid or another error occurred.
 */
extern struct job_descriptor_t*
PyMoxie_JobDescriptor_For_Id
(
    PyMoxie_InternalJobScheduler *scheduler,
    job_id_t                         job_id
);

#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif /* __MOXIE__MOXIE_CORE_MOXIE_CORE_H__ */
