/**
 * scheduler.h: Defines the interface to a task scheduling system intended to 
 * support efficient distribution of work across host hardware threads.
 */
#ifndef __MOXIE_CORE_SCHEDULER_H__
#define __MOXIE_CORE_SCHEDULER_H__

#pragma once

#ifndef MOXIE_CORE_NO_INCLUDES
#   include <stddef.h>
#   include <stdint.h>
#   include <stdbool.h>
#endif

/**
 * Define the size of a single cacheline on the target architecture, in bytes.
 * This value may be used for padding purposes to prevent false sharing.
 */
#ifndef CACHELINE_SIZE
#   if   defined(__aarch64__) || defined(_M_ARM64)
#       define CACHELINE_SIZE                                                  64
#   elif defined(__arm__) || defined(_M_ARM)
#       define CACHELINE_SIZE                                                  64
#   elif defined(__amd64__) || defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
#       define CACHELINE_SIZE                                                  64
#   elif defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) || defined(_M_IX86) || defined(_X86_)
#       define CACHELINE_SIZE                                                  64
#   elif defined(__ppc__) || defined(__powerpc__) || defined(__PPC__)
#       define CACHELINE_SIZE                                                  128
#   else
#       error Unknown target architecture, please define CACHELINE_SIZE
#   endif
#endif /* CACHELINE_SIZE */

/**
 * Constants used by the threading wrapper.
 * THREAD_ID_INVALID         : The value representing an invalid thread identifier. Implicitly converable to bool.
 * THREAD_STACK_SIZE_DEFAULT : The default number of bytes reserved for a thread's stack space.
 */
#ifndef THREADING_CONSTANTS
#   define THREADING_CONSTANTS
#   define THREAD_ID_INVALID                                                  ((thread_id_t)0)
#   define THREAD_STACK_SIZE_DEFAULT                                           (4UL * 1024UL * 1024UL)
#endif /* THREADING_CONSTANTS */

/**
 * Constants used by the job scheduler system.
 * JOB_ID_INVALID            : The value representing an invalid job identifier. Implicitly converable to bool.
 * JOB_NAMESPACE_COUNT_MAX   : The maximum number of namespaces (worker pools) that can be defined.
 * JOB_COUNT_MAX             : The maximum number of jobs created at any one time. This value must be a power of two.
 * JOB_WAITER_COUNT_MAX      : The maximum number of jobs that can be waiting on a single job to complete before they can start.
 * JOB_CONTEXT_RTRQ_MAX      : The capacity of the context-local ready-to-run queue. This value must be a power of two.
 * JOB_BUFFER_JOB_COUNT      : The maximum number of jobs that can be allocated from a single job buffer.
 * JOB_BUFFER_SIZE_BYTES     : The maximum number of bytes that can be allocated from a single job buffer.
 * JOB_WAITER_LIST_SIZE_BYTES: The number of bytes per-job allocated to the waiter list.
 */
#ifndef JOB_SCHEDULER_CONSTANTS
#   define JOB_SCHEDULER_CONSTANTS
#   define JOB_ID_INVALID                                                       0U
#   define JOB_ID_VALID_BITS                                                    1
#   define JOB_ID_INDEX_BITS                                                    16
#   define JOB_ID_GENER_BITS                                                    15
#   define JOB_ID_VALID_SHIFT                                                   0
#   define JOB_ID_INDEX_SHIFT                                                   1
#   define JOB_ID_GENER_SHIFT                                                   17
#   define JOB_ID_VALID_MASK                                                    0x00000001U
#   define JOB_ID_INDEX_MASK                                                    0x0000FFFFU
#   define JOB_ID_GENER_MASK                                                    0x00007FFFU
#   define JOB_ID_VALID_MASK_PACKED                                             0x00000001U
#   define JOB_ID_INDEX_MASK_PACKED                                             0x0001FFFEU
#   define JOB_ID_GENER_MASK_PACKED                                             0xFFFE0000U
#   define JOB_NAMESPACE_COUNT_MAX                                              16
#   define JOB_COUNT_MAX                                                        65536
#   define JOB_WAITER_COUNT_MAX                                                 32
#   define JOB_CONTEXT_RTRQ_MAX                                                 64
#   define JOB_BUFFER_JOB_COUNT                                                 64
#   define JOB_BUFFER_SIZE_BYTES                                               (JOB_BUFFER_JOB_COUNT * 1024)
#   define JOB_STATUS_WAITER_LIST_SIZE_BYTES                                   (JOB_WAITER_COUNT_MAX * sizeof(uint16_t))
#endif /* JOB_SCHEDULER_CONSTANTS */

#ifndef JOB_ID_HELPERS_DEFINED
#   define JOB_ID_HELPERS_DEFINED
/**
 * Test whether a job_id_t contains a valid job identifier.
 */
#   define job_id_valid(_id)                                                   \
        (((_id) & JOB_ID_VALID_MASK_PACKED) != 0)

/**
 * Extract the job descriptor slot index from a job identifier.
 * The caller must ensure that the job_id_t value is a valid job ID.
 */
#   define job_id_get_slot_index_u32(_id)                                      \
        (((_id) & JOB_ID_INDEX_MASK_PACKED) >> JOB_ID_INDEX_SHIFT)

/**
 * Extract the job descriptor slot generation from a job identifier.
 * The caller must ensure that the job_id_t value is a valid job ID.
 */
#   define job_id_get_generation_u32(_id)                                      \
        (((_id) & JOB_ID_GENER_MASK_PACKED) >> JOB_ID_GENER_SHIFT)

/**
 * Build a valid job_id_t value from its constituient parts.
 * The job identifier has its valid bit set. Use JOB_ID_INVALID to represent an invalid identifier.
 */
#   define job_id_pack(_desc_index_u32, _generation_u32)                       \
        ((((_desc_index_u32) & JOB_ID_INDEX_MASK) << JOB_ID_INDEX_SHIFT) |     \
         (((_generation_u32) & JOB_ID_GENER_MASK) << JOB_ID_GENER_SHIFT) |     \
         (((1U             ) & JOB_ID_VALID_MASK) << JOB_ID_VALID_SHIFT))
#endif /* JOB_ID_HELPERS_DEFINED */

struct  job_queue_t;
struct  job_buffer_t;
struct  job_context_t;
struct  job_scheduler_t;
struct  job_descriptor_t;

typedef uint32_t   job_id_t;                                                   /* Job identifiers are simply 32-bit unsigned integers. */
typedef uint64_t   thread_id_t;                                                /* Alias for an operating system thread identifier. */
typedef uint32_t (*PFN_thread_start)(void*);                                   /* Function pointer type for operating system thread entry point. */
typedef int32_t  (*PFN_job_start   )(                                          /* Function pointer type for job entry point. */
    struct job_context_t   *,                                                  /* The context bound to the thread on which the job is executing. */
    struct job_descriptor_t*                                                   /* Pointer to the job descriptor in the scheduler's job list. */
);

typedef enum   job_queue_signal_e {                                            /* Defined values for job queue signals, which can be sent to wake all waiting threads. */
    JOB_QUEUE_SIGNAL_CLEAR                                       =  0U,        /* The current signal status should be cleared. */
    JOB_QUEUE_SIGNAL_TERMINATE                                   =  1U,        /* Threads should terminate. */
    JOB_QUEUE_SIGNAL_USER                                        =  2U,        /* The first signal value available for application use. */
} job_queue_signal_e;

typedef enum   job_submit_type_e {                                             /* The various job submission types. Typically, a job is submitted to run, but if an error has occurred the job should be submitted in a canceled state. */
    JOB_SUBMIT_RUN                                               =  0L,        /* Submit the job for execution. */
    JOB_SUBMIT_CANCEL                                            = -1L,        /* Cancel the job. */
} job_submit_type_e;

typedef enum   job_submit_result_e {
    JOB_SUBMIT_SUCCESS                                           =  0L,        /* The job was submitted successfully and will run when ready. */
    JOB_SUBMIT_INVALID_JOB                                       = -1L,        /* The job was rejected because some of the job data was invalid. */
    JOB_SUBMIT_TOO_MANY_WAITERS                                  = -2L,        /* The job was rejected because one of its dependencies has too many jobs waiting on it. */
} job_submit_result_e;

typedef enum   job_context_id_e {                                              /* Define reserved job context identifiers. */
    JOB_CONTEXT_ID_MAIN                                          =  0U,        /* Identifies the main process thread. */
    JOB_CONTEXT_ID_USER                                          =  1U,        /* The first available user-defined context identifier. */
} job_context_id_e;

typedef enum   job_state_e {                                                   /* Define the possible states for a job. */
    JOB_STATE_UNINITIALIZED                                      =  0,         /* The job data has not been initialized. This value MUST be zero. */
    JOB_STATE_NOT_SUBMITTED                                      =  1,         /* The job has been created but not yet submitted. */
    JOB_STATE_NOT_READY                                          =  2,         /* The job has been submitted but it not yet ready-to-run. */
    JOB_STATE_READY                                              =  3,         /* The job is ready-to-run but no worker thread has started it yet. */
    JOB_STATE_RUNNING                                            =  4,         /* The job is currently executing. */
    JOB_STATE_COMPLETED                                          =  5,         /* The job has finished executing. */
    JOB_STATE_CANCELED                                           =  6,         /* The job has been canceled. */
} job_state_e;

typedef struct job_context_id_t {                                              /* Data uniquely identifying a specific job context. */
    uint32_t                         id;                                       /* The identifier of the namespace to which the context belongs. */
    uint32_t                        idx;                                       /* The zero-based index of the context within the namespace. */
} job_context_id_t;

typedef struct job_context_namespace_t {                                       /* Defines a job "namespace" which is essentially a collection of pairs of worker thread + `job_context_t`. */
    uint32_t                         id;                                       /* An application-defined identifier for the context IDs, which must be >= JOB_CONTEXT_ID_USER. */
    uint32_t                        num;                                       /* The number of contexts/worker threads to allocate in the namespace. */
    size_t                        stack;                                       /* The size of the thread's stack, in bytes. May be THREAD_STACK_SIZE_DEFAULT, or 0 if id is JOB_CONTEXT_ID_MAIN. */
    PFN_thread_start               main;                                       /* The entry point routine for the thread. May be NULL if id is JOB_CONTEXT_ID_MAIN. */
    char const                    *name;                                       /* Pointer to a nul-terminated string literal specifying a human-readable name for the namespace. */
} job_context_namespace_t;

typedef struct job_scheduler_config_t {                                        /* Data used to configure a job scheduler. */
    struct job_queue_t         **queues;                                       /* An array of namespace_count job queue references specifying the global work queues the contexts for each namespace park on. The same queue may be used across multiple namespaces. */
    job_context_namespace_t *namespaces;                                       /* An array of namespace_count namespace definitions used to pre-initialize the job execution contexts. */
    size_t              namespace_count;                                       /* The maximum number of job_context_t structures the application will ever allocate directly, typically one for each externally-managed worker thread. */
    size_t        prealloc_jobbuf_count;                                       /* The number of job buffers to pre-allocate when the scheduler is created. If zero, no job buffers are pre-allocated. */
} job_scheduler_config_t;

typedef struct job_descriptor_t {                                              /* Information about an allocated job. Should be padded to cacheline size. */
    struct job_buffer_t         *jobbuf;                                       /* The job buffer that owns the data associated with the job. */
    struct job_queue_t          *target;                                       /* The job queue to which the job should be submitted when it becomes ready-to-run. */
    PFN_job_start               jobmain;                                       /* The function defining the job implementation. */
    uintptr_t                     user1;                                       /* An opaque application-defined value that can be used for specifying data associated with the job. */
    uintptr_t                     user2;                                       /* An opaque application-defined value that can be used for specifying data associated with the job. */
    void                          *data;                                       /* Pointer to the start of the data region allocated to the job. */
    uint32_t                       size;                                       /* The maximum number of bytes that can be written to the job data region. */
    job_id_t                         id;                                       /* The identifier of the job. May be JOB_ID_INVALID. */
    job_id_t                     parent;                                       /* The identifier of the job's parent task. Set to JOB_ID_INVALID if the job has no parent. */
    int32_t                        exit;                                       /* The exit code returned by the job entry point. */
} job_descriptor_t;

typedef struct job_buffer_t {                                                  /* Manages underlying storage for job data. Recycled on a free list. */
    struct job_buffer_t           *next;                                       /* Pointer to the next node in the free list. May be null.  */
    uint8_t                    *membase;                                       /* Pointer to the first addressable byte of memory managed by the buffer. */
    size_t                       nxtofs;                                       /* The zero-based index of the next available byte within the job buffer. */
    size_t                       maxofs;                                       /* The maximum value of the next_offset field (the capacity of the job buffer, in bytes). */
    uint32_t                    jobbase;                                       /* The zero-based index of the first job descriptor slot allocated to jobs from this job buffer. */
    uint32_t                     refcnt;                                       /* The buffer reference count. Incremented once when the buffer is allocated, +1 for each un-completed job allocated from the buffer. */
} job_buffer_t;

typedef struct job_context_t {                                                 /* Per-thread data required to define and execute jobs. */
    struct job_queue_t           *queue;                                       /* The ready-to-run queue on which this context/thread waits for work. */
    struct job_buffer_t         *jobbuf;                                       /* The job buffer from which the context allocates. */
    struct job_scheduler_t       *sched;                                       /* A pointer back to the scheduler instance that created the context. */
    thread_id_t                   thrid;                                       /* The identifier of the thread that owns the context. This is the only thread that can create, submit and execute jobs using this context. */
    uint32_t                     jobcnt;                                       /* The number of jobs allocated from this context's current job buffer. */
    uint32_t                      ctxid;                                       /* The application-defined identifier bound to the context at allocation time. */
    uint32_t                   ctxindex;                                       /* The zero-based index of the context within the type namespace. */
    uint32_t                   arrindex;                                       /* The zero-based index of the context within the scheduler's context list. */
    char const                    *name;                                       /* The name of the context namespace (used for debugging purposes only). */
    uint64_t                       pad1;                                       /* Reserved for future use. Set to zero. */
} job_context_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Retrieve the number of logical processors in the host system.
 */
extern uint32_t
logical_processor_count
(
    void
);

/**
 * Retrieve the identifier of the calling thread.
 */
extern thread_id_t 
current_thread_id
(
    void
);

/**
 * Create and launch a new operating system thread with a user-specified entry point.
 * The thread runs until the supplied entry point routine returns. The thread is joinable.
 * @param entry_point The function serving as the main entry point for the thread.
 * @param stack_size The desired minimum stack size for the thread, in bytes. May be THREAD_STACK_SIZE_DEFAULT.
 * @param argp Opaque data to pass through to the entry point function.
 * @return The identifier of the new thread, or THREAD_ID_INVALID if the thread could not be launched.
 */
extern thread_id_t
thread_create
(
    PFN_thread_start entry_point,
    size_t            stack_size,
    void                   *argp
);

/**
 * Block the calling thread until a given thread terminates.
 * @param thread_id The operating system thread to wait for.
 * @return The exit code of the thread.
 */
extern uint32_t
thread_join
(
    thread_id_t thread_id
);

/**
 * Allocate and initialize to empty a new fixed-length, waitable queue for storing ready-to-run jobs.
 * The queue capacity is always JOB_COUNT_MAX, and the elements stored are always pointers to `job_descriptor_t`.
 * @param queue_id An application-defined identifier for the queue.
 * @return The new, empty queue, or NULL of resource allocation failed.
 */
extern struct job_queue_t*
job_queue_create
(
    uint32_t queue_id
);

/**
 * Free resources associated with a waitable job queue.
 * @param queue The waitable job queue to delete.
 */
extern void
job_queue_delete
(
    struct job_queue_t *queue
);

/**
 * Flush the queue and wake all waiting producer threads.
 * @param queue The waitable job queue to flush.
 */
extern void
job_queue_flush
(
    struct job_queue_t *queue
);

/**
 * Signal all waiters on a queue to wake up for some event like process shutdown.
 * All waiting producer threads and all waiting consumer threads are woken up.
 * The signal remains set until it is cleared using `job_queue_clear_signal`.
 * @param queue The waitable queue to signal.
 * @param signal The signal value to set. A value of JOB_QUEUE_SIGNAL_CLEAR allows threads to park on the queue again.
 */
extern void
job_queue_signal
(
    struct job_queue_t *queue,
    uint32_t           signal
);

/**
 * Check the signal status of a waitable job queue.
 * @param queue The waitable job queue to query.
 * @return The value of the job queue signal. If the queue is not signaled, the return value is JOB_QUEUE_SIGNAL_CLEAR.
 */
extern uint32_t
job_queue_check_signal
(
    struct job_queue_t *queue
);

/**
 * Retrieve the identifier of a waitable job queue.
 * @param queue The waitable job queue to query.
 * @return The application-defined identifier of the job queue.
 */
extern uint32_t
job_queue_get_id
(
    struct job_queue_t *queue
);

/**
 * Push a ready-to-run job onto a job queue.
 * This operation may wake one or more waiting threads.
 * @param queue The target queue. One of the threads that services this queue will process the job.
 * @param job The job to enqueue. The job must have a `wait` value of 0.
 * @return One of the item was pushed onto the queue, or zero if the queue is signaled.
 */
extern uint32_t
job_queue_push
(
    struct job_queue_t    *queue, 
    struct job_descriptor_t *job
);

/**
 * Take a ready-to-run job from a job queue.
 * If the queue is empty, the calling thread is blocked until a job becomes ready-to-run or the queue is signaled.
 * @param queue The queue to take from.
 * @return A pointer to the dequeued job descriptor, or NULL if the queue was signaled.
 */
extern struct job_descriptor_t*
job_queue_take
(
    struct job_queue_t    *queue
);

/**
 * Allocate and initialize storage for a job scheduler instance.
 * @param config Data used to configure the attributes of the job scheduler.
 * @return A pointer to the new scheduler instance, or NULL if an error occurred.
 */
extern struct job_scheduler_t*
job_scheduler_create
(
    struct job_scheduler_config_t *config
);

/**
 * Free resources allocated to a job scheduler instance.
 * The caller is responsible for ensuring any worker threads are stopped.
 * @param scheduler The job scheduler to delete.
 */
extern void
job_scheduler_delete
(
    struct job_scheduler_t *scheduler
);

/**
 * Send JOB_QUEUE_SIGNAL_TERMINATE to all queues associated with a job scheduler.
 * The caller should then join all worker threads with the main process, at which point job_scheduler_delete can be called.
 * @param scheduler The job scheduler to terminate.
 */
extern void
job_scheduler_terminate
(
    struct job_scheduler_t *scheduler
);

/**
 * Assign a thread as the owner of a particular job execution context.
 * This function can be called to re-assign the owner of a job context in case a worker thread crashes.
 * @param scheduler The job scheduler managing the job execution context to claim or assign.
 * @param namespace_id An application-defined identifier for the worker namespace, specified when the scheduler was created in job_context_namespace_t::id.
 * @param worker_index A zero-based index of the worker thread, in the range [0, job_context_namespace_t::num).
 * @param owner_tid The identifier of the thread that is taking ownership of the job context.
 * @return A pointer to the associated job context, or NULL if the pair (namespace_id, worker_index) to not identify a known job context.
 */
extern struct job_context_t*
job_scheduler_assign_context
(
    struct job_scheduler_t *scheduler,
    uint32_t             namespace_id,
    uint32_t             worker_index,
    thread_id_t             owner_tid
);

/**
 * Retrieve the job context bound to a specific identifier.
 * @param scheduler The job scheduler managing the job execution context to claim or assign.
 * @param namespace_id An application-defined identifier for the worker namespace, specified when the scheduler was created in job_context_namespace_t::id.
 * @param worker_index A zero-based index of the worker thread, in the range [0, job_context_namespace_t::num).
 * @return A pointer to the associated job context, or NULL if the pair (namespace_id, worker_index) to not identify a known job context.
 */
extern struct job_context_t*
job_scheduler_get_context
(
    struct job_scheduler_t *scheduler,
    uint32_t             namespace_id,
    uint32_t             worker_index
);

/**
 * Retrieve a reference to the job queue with a given identifier.
 * The queue must have been specified in job_context_namespace_t::queues when the scheduler was created.
 * @param scheduler The job scheduler to query.
 * @param queue_id The application-defined identifier of the queue to retrieve.
 * @return A pointer to the requested job queue, or NULL if the queue could not be found.
 */
extern struct job_queue_t*
job_scheduler_get_queue
(
    struct job_scheduler_t *scheduler,
    uint32_t                 queue_id
);

/**
 * Retrieve the number of job contexts (~number of worker threads) that process work from a given queue.
 * The queue must have been specified in job_context_namespace_t::queues when the scheduler was created.
 * @param scheduler The job scheduler to query.
 * @param queue_id The application-defined identifier of the job queue.
 * @return The number of job contexts (~number of worker threads) that process work from the specified job queue.
 */
extern uint32_t
job_scheduler_get_queue_worker_count
(
    struct job_scheduler_t *scheduler,
    uint32_t                 queue_id
);

/**
 * Attempt to cancel a job.
 * If the job has already started, it is allowed to complete.
 * @param scheduler The job scheduler managing the job.
 * @param id The job identifier.
 * @return One of the values of the job_state_e enumeration specifying the current state of the job.
 */
extern int
job_scheduler_cancel
(
    struct job_scheduler_t *scheduler,
    job_id_t                       id
);

/**
 * Retrieve a reference to the scheduler that created a particular job context.
 * @param context The context to query.
 * @return The scheduler that created the context.
 */
extern struct job_scheduler_t*
job_context_scheduler
(
    struct job_context_t   *context
);

/**
 * Retrieve a reference to the queue to which a job context submits ready-to-run jobs.
 * @param context The context to query.
 * @return The job queue bound to the context.
 */
extern struct job_queue_t*
job_context_queue
(
    struct job_context_t   *context
);

/**
 * Retrieve a unique identifier for a job context.
 * @param context The context to query.
 * @return The unique identifier for the context.
 */
extern struct job_context_id_t
job_context_id
(
    struct job_context_t   *context
);

/**
 * Retrieve the identifier of the thread that owns a job context.
 * @param context The context to query.
 * @return The identifier of the thread that owns the context, or THREAD_ID_INVALID if no thread has claimed the context using job_scheduler_assign_context.
 */
extern thread_id_t
job_context_thread_id
(
    struct job_context_t   *context
);

/**
 * Allocate storage for a job. The job cannot run until `job_context_submit_job` is called.
 * This function should only ever be called from the thread that owns the specified job context.
 * @param context The job context from which the job will be allocated.
 * @param data_size The number of bytes of data required to store job attributes.
 * @param data_align The required alignment of the job data. This must be a non-zero power of two.
 * @return A pointer to the job descriptor which can be used to write the job data, or `NULL` if the allocation request cannot be satisfied.
 * The caller should set the fields of the returned job descriptor as appropriate:
 * - target: Set to the queue to which the job should be submitted. A value of `NULL` will submit to the queue bound to the context.
 * - main  : Set to the function to execute when the job runs.
 * - data  : A pointer to the start of the buffer where up to size bytes can be written.
 * - parent: The identifier of the parent job, if any, or `JOB_ID_INVALID` if there is no parent job.
 * The job creation process initializes all fields to their default values. The job identifier is returned in the `job_descriptor_t::id` field.
 */
extern struct job_descriptor_t*
job_context_create_job
(
    struct job_context_t   *context,
    size_t                data_size,
    size_t               data_align
);

/**
 * Submit a job to execute or for cancellation.
 * This function should only ever be called from the thread that owns the specified job context.
 * @param context The job context from which the job was allocated.
 * @param job The job descriptor returned by the prior call to `job_context_create_job`.
 * @param dependency_list The list of identifiers of jobs that must complete before the job being submitted can run. Specify `NULL` for an empty list.
 * @param dependency_count The number of identifiers in `dependency_list`. Specify 0 for an empty list.
 * @param submit_type One of the values of the `job_submit_type_e` enumeration, indicating whether the job should be submitted to run or whether it should be canceled.
 * @return One of the values of the `job_submit_result_e` enumeration, indicating whether job submission was successful.
 */
extern int
job_context_submit_job
(
    struct job_context_t   *context,
    struct job_descriptor_t    *job,
    job_id_t const *dependency_list,
    size_t         dependency_count,
    int                 submit_type
);

/**
 * Attempt to cancel a previously submitted job.
 * This operation is not guaranteed to succeed; if the job has already started running it will not be stopped.
 * @param context The job context bound to the calling thread.
 * @param id The identifier of the job to cancel.
 * @return One of the values of the job_state_e enumeration specifying the current state of the job.
 */
extern int
job_context_cancel_job
(
    struct job_context_t   *context,
    job_id_t                     id
);

/**
 * Perform a busy wait for a job to complete.
 * While waiting, the calling thread will attempt to execute pending jobs from the context's wait queue.
 * @param context The job context bound to the calling thread.
 * @param id The identifier of the job to wait for.
 * @return Non-zero if the specified job has completed, or zero if the wait queue is signaled or the job ID is invalid.
 */
extern int
job_context_wait_job
(
    struct job_context_t *context,
    job_id_t                   id
);

/**
 * Wait for a ready-to-run job to be available on the wait queue bound to a context.
 * If no jobs are available, the calling thread sleeps until a job is available or the queue is signaled.
 * This function will never return a canceled job - a canceled job will be immediately completed and the queue will be polled again.
 * @param context The job context bound to the calling thread.
 * @return The `job_descriptor_t` for the ready-to-run job, or NULL if the queue was signaled or the timeout interval elapsed.
 */
extern struct job_descriptor_t*
job_context_wait_ready_job
(
    struct job_context_t *context
);

/**
 * Signal completion of a job after its entry point has returned. This may make additional jobs ready-to-run.
 * @param context The job context bound to the calling thread (the thread that executed the job).
 * @param job The descriptor for the completed job.
 */
extern void
job_context_complete_job
(
    struct job_context_t *context,
    struct job_descriptor_t  *job
);

#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif /* __MOXIE_CORE_SCHEDULER_H__ */
