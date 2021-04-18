#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <pthread.h>
#include <execinfo.h>
#include <sys/mman.h>

#include "moxie/scheduler.h"

/**
 * Return the byte alignment of a given type.
 */
#ifdef _MSC_VER
#   define SCHEDULER_alignof_type(_type)                                       \
    __alignof(_type)
#else
#   define SCHEDULER_alignof_type(_type)                                       \
    __alignof__(_type)
#endif

typedef struct job_queue_posix_t {                                             /* Data associated with a waitable job queue. */
    struct job_descriptor_t **storage;                                         /* Storage for queue entries. Capacity is always JOB_COUNT_MAX. */
    uint64_t               push_count;                                         /* The number of push operations that have occurred against the queue. */
    uint64_t               take_count;                                         /* The number of take operations that have occurred against the queue. */
    uint32_t                   signal;                                         /* Set to non-zero if the queue is in a signaled state due to an external event. */
    uint32_t                 queue_id;                                         /* The application-defined identifier for the queue. */
    pthread_mutex_t             mutex;                                         /* The mutex used to synchronize access to the queue. */
    pthread_cond_t        consumer_cv;                                         /* The condition variable used to park consumer threads when the queue is empty. */
    pthread_cond_t        producer_cv;                                         /* The condition variable used to park producer threads when the queue is full. */
} job_queue_posix_t;

typedef struct job_data_posix_t {                                              /* Internal data associated with each job. */
    pthread_mutex_t              lock;                                         /* The mutex used to synchronize access to job execution data. */
    uint16_t                 *waiters;                                         /* Pointer to an array of JOB_WAITER_COUNT_MAX job slot indices used to register waiters. */
    uint32_t                  waitcnt;                                         /* The number of valid entries in the waiters array (max JOB_WAITER_COUNT_MAX). */
    int32_t                      wait;                                         /* The number of jobs that must complete before this job becomes ready-to-run (number of uncompleted dependencies). */
    int32_t                      work;                                         /* The number of jobs that must complete before this job can complete (number of uncompleted children+1 for self). */
    int32_t                     state;                                         /* One of the values of the job_state_e enumeration identifying the current state of the job. */
} job_data_posix_t;

typedef struct job_scheduler_posix_t {                                         /* Data maintained by a scheduler instance. Primarily book-keeping data. */
    uint64_t                  memsize;                                         /* The size of the allocated memory region, in bytes. */
    job_descriptor_t         *jobdesc;                                         /* Storage for common job book-keeping attributes. Capacity is JOB_COUNT_MAX, random access. */
    job_data_posix_t         *jobdata;                                         /* Internal data representing job execution state. Capacity is JOB_COUNT_MAX, random access. */
    job_buffer_t             **jobbuf;                                         /* List of pointers to all currently allocated job buffers. Capacity is jobctx_limit, items in use jobctx_count. */

    pthread_rwlock_t     queue_rwlock;                                         /* Reader-writer lock to protect queue information. */
    size_t                queue_count;                                         /* The number of unique queues across which jobs are scheduled. */
    struct job_queue_t    *job_queues[JOB_QUEUE_COUNT_MAX];                    /* The subset of unique queues across which jobs are scheduled. */
    uint32_t                queue_ids[JOB_QUEUE_COUNT_MAX];                    /* List of queue identifiers for each item in job_queues. */
    uint32_t               queue_refs[JOB_QUEUE_COUNT_MAX];                    /* List of reference counts for each queue (number of worker threads that publish to and/or wait on that queue). */

    pthread_rwlock_t    jobctx_rwlock;                                         /* Reader-writer lock to protect job context-related fields which are mostly read-only. */
    job_context_t       *jobctx_flist;                                         /* The head of the free list of job_context_t. */
    size_t               jobctx_count;                                         /* Number of valid entries in the appids, ctxids and jobctx arrays. */

    pthread_mutex_t      jobbuf_mutex;                                         /* The mutex protecting the jobbuf_flist and jobbuf_count fields. */
    job_buffer_t        *jobbuf_flist;                                         /* Head node of the free list of job buffers. */
    size_t               jobbuf_limit;                                         /* The capacity of the jobbuf array. */
    size_t               jobbuf_count;                                         /* The number of valid entries in the jobbuf array. */
} job_scheduler_posix_t;

typedef struct thread_arg_posix_t {                                            /* Data supplied to pthread_wrapper_entry when a thread is launched via thread_create. */
    PFN_thread_start             main;                                         /* The actual user-supplied entry point routine for the thread.*/
    void                        *argp;                                         /* The actual user-supplied data to be passed to the entry point routine. */
} thread_arg_posix_t;

/**
 * Increment a 32-bit unsigned integer.
 * @param address The address of the value to increment.
 * @return The resulting incremented value.
 */
static uint32_t
atomic_increment_u32
(
    uint32_t volatile *address
)
{
    return __atomic_add_fetch(address, 1, __ATOMIC_SEQ_CST);
}

/**
 * Decrement a 32-bit unsigned integer value.
 * @param address The address of the value to decrement.
 * @return The resulting decremented value.
 */
static uint32_t
atomic_decrement_u32
(
    uint32_t volatile *address
)
{
    return __atomic_sub_fetch(address, 1, __ATOMIC_SEQ_CST);
}

/**
 * Create and allocate resources for a new job buffer.
 * @param buffer_index The zero-based global index of the job buffer (the index of the buffer in the job_scheduler_t::jobbuf array).
 * @return A pointer to the new job buffer instance, or NULL if memory allocation failed.
 */
static struct job_buffer_t*
job_buffer_create
(
    size_t buffer_index
)
{
    job_buffer_t *jobbuf = NULL;
    void         *jobmem = NULL;
    int           access = PROT_READ   | PROT_WRITE;
    int            flags = MAP_PRIVATE | MAP_ANONYMOUS;

    if ((jobbuf =(job_buffer_t*) malloc(sizeof(job_buffer_t))) == NULL) {
        return NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              
    }
    if ((jobmem = mmap(NULL, JOB_BUFFER_SIZE_BYTES, access, flags, -1, 0)) != MAP_FAILED) {
        jobbuf->next       = NULL;
        jobbuf->membase    =(uint8_t*) jobmem;
        jobbuf->nxtofs     = 0;
        jobbuf->maxofs     = JOB_BUFFER_SIZE_BYTES;
        jobbuf->jobbase    =(uint32_t)(JOB_BUFFER_JOB_COUNT * buffer_index);
        jobbuf->refcnt     = 0;
        return jobbuf;
    } else { /* Failed to allocate job memory */
        free(jobbuf);
        return NULL;
    }
}

/**
 * Free resources allocated to a job buffer.
 * @param jobbuf The job buffer to delete.
 */
static void
job_buffer_delete
(
    struct job_buffer_t *jobbuf
)
{
    if (jobbuf != NULL) {
        if (jobbuf->membase != NULL) {
            int res = munmap(jobbuf->membase, JOB_BUFFER_SIZE_BYTES);
            assert(res == 0 && "Call to munmap for job buffer memory failed");
            (void) sizeof(res); /* Unused local */
            jobbuf->membase = NULL;
        }
        jobbuf->nxtofs = 0;
        jobbuf->maxofs = 0;
        free(jobbuf);
    }
}

/**
 * Acquire a job buffer, possibly releasing a previously-acquired job buffer.
 * This function should be called when creating a job, and the job context's job count has reached the maximum value or allocation from the current job buffer fails.
 * @param sched The scheduler instance from which the job buffer should be allocated.
 * @param release Optional pointer to the job buffer currently held by the job context.
 * @return A pointer to the new job buffer, or NULL if an error occurred.
 */
static struct job_buffer_t*
jobbuf_acquire
(
    struct job_scheduler_posix_t *sched,
    struct job_buffer_t        *release
)
{
    job_buffer_t *acquired = NULL;
    uint32_t        refcnt = 0;

    if (release != NULL) {
        if ((refcnt  = atomic_decrement_u32(&release->refcnt)) == 0) {
            acquired = release; /* Would return release to free list, so just return it */
        } /* Else, don't do anything - there are still outstanding references */
    }
    if (acquired == NULL && pthread_mutex_lock(&sched->jobbuf_mutex) == 0) {
        if ((acquired = sched->jobbuf_flist) != NULL) { /* Popped from free list */
            sched->jobbuf_flist = acquired->next;
        } else { /* Free list empty, allocate a new buffer */
            if (sched->jobbuf_count < sched->jobbuf_limit) {
                if ((acquired = job_buffer_create(sched->jobbuf_count)) != NULL) {
                    /* Append new buffer to array of all buffers */
                    sched->jobbuf[sched->jobbuf_count++] = acquired;
                } else {
                    (void) pthread_mutex_unlock(&sched->jobbuf_mutex);
                    return NULL; /* Failed to allocate buffer */
                }
            } else {
                assert(sched->jobctx_count < sched->jobbuf_limit && "Out of job buffers");
                (void) pthread_mutex_unlock(&sched->jobbuf_mutex);
                return NULL; /* All possible job buffers have been allocated */
            }
        }
        (void) pthread_mutex_unlock(&sched->jobbuf_mutex);
    }
    acquired->next   = NULL;
    acquired->nxtofs = 0;
    acquired->refcnt = 1;
    return acquired;
}

/**
 * Release a reference to a job buffer.
 * This function should be called when a job has completed and no longer needs to access its data
 * @param sched The scheduler instance from which the job buffer was acquired.
 * @param release The job buffer to release.
 */
static void
jobbuf_release
(
    struct job_scheduler_posix_t *sched, 
    struct job_buffer_t        *release
)
{
    uint32_t refcnt;

    if (release != NULL) {
        if ((refcnt = atomic_decrement_u32(&release->refcnt)) == 0) {
            /* No outstanding references, so return to the free list */
            if (pthread_mutex_lock(&sched->jobbuf_mutex) == 0) {
                release->next = sched->jobbuf_flist;
                sched->jobbuf_flist = release;
                (void) pthread_mutex_unlock(&sched->jobbuf_mutex);
            }
        }
    }
}

/**
 * Attempt to allocate memory from a job buffer.
 * @param jobbuf The job buffer to allocate from.
 * @param size The minimum number of bytes to required by the caller.
 * @param align The desired alignment of the returned address. This must be a non-zero power of two.
 * @return A pointer to the start of the allocated block of at least size bytes, or NULL if the request cannot be satisfied.
 */
static void*
jobbuf_alloc
(
    struct job_buffer_t *jobbuf,
    size_t                 size,
    size_t                align
)
{
    uint8_t    *base_address = jobbuf->membase + jobbuf->nxtofs;
    uintptr_t   address_uint =(uintptr_t)  base_address;
    uint8_t *aligned_address =(uint8_t *)((address_uint + (align - 1)) & ~(align - 1));
    size_t       align_bytes =(size_t   ) (aligned_address - base_address);
    size_t       alloc_bytes =(size_t   ) (size + align_bytes);
    size_t        new_offset =(size_t   ) (jobbuf->nxtofs + alloc_bytes);

    if (new_offset <= jobbuf->maxofs) {
        jobbuf->nxtofs = new_offset;
        return aligned_address;
    } return NULL;
}

/**
 * The wrapper entry point conforming to pthreads expectations for all threads launched with `thread_create`.
 * @param argp A pointer to an instance of thread_arg_posix_t, allocated on the stack of `thread_create`.
 * @return The thread exit code, cast to void*.
 */
static void*
pthread_wrapper_entry
(
    void *argp
)
{
    thread_arg_posix_t *args =(thread_arg_posix_t*) argp;
    PFN_thread_start th_main = args->main; /* Copy to thread stack */
    void               *argv = args->argp; /* Copy to thread stack */
    uintptr_t      exit_code = EXIT_FAILURE;
    exit_code = th_main(argv);
    return (void*) exit_code;
}

/**
 * A default no-op entry point that can be specified for a job.
 * @param ctx The `job_context_t` bound to the thread executing the job.
 * @param job The `job_descriptor_t` specifying data about the executing job.
 * @return An application-specific exit code, by default, `EXIT_SUCCESS`.
 */
static int32_t
default_job_main
(
    struct job_context_t    *ctx,
    struct job_descriptor_t *job
)
{
    (void) sizeof(ctx); /* Unused */
    (void) sizeof(job); /* Unused */
    return EXIT_SUCCESS;
}

uint32_t
logical_processor_count
(
    void
)
{
    long nprocs_raw = sysconf(_SC_NPROCESSORS_ONLN);
    uint32_t nprocs =(uint32_t) nprocs_raw;
    if (nprocs_raw <= 0) {
        nprocs      = 1;
    } return nprocs;
}

thread_id_t 
current_thread_id
(
    void
)
{
    return (thread_id_t) pthread_self();
}

thread_id_t
thread_create
(
    PFN_thread_start entry_point,
    size_t            stack_size,
    void                   *argp
)
{
    thread_arg_posix_t tstart = {};
    pthread_t       thread_id = THREAD_ID_INVALID;
    pthread_attr_t       attr = {};
    const size_t    STACK_MIN = 16384; /* PTHREAD_STACK_MIN undefined on some systems */
    size_t          page_size = 4096;
    long          page_size_r = sysconf(_SC_PAGESIZE);
    int                   res = 0;

    if (entry_point == NULL) {
        assert(entry_point != NULL && "Expected non-null entry_point argument");
        return THREAD_ID_INVALID;
    }
    if (stack_size == 0) {
        stack_size  = THREAD_STACK_SIZE_DEFAULT;
    }
    if (stack_size < STACK_MIN) {
        stack_size = STACK_MIN;
    }
    if (page_size_r == -1) { /* Try 4KiB and hope for the best */
        page_size = 4096;
    } else {
        page_size =(size_t) page_size_r;
    }
    /* Round requested stack size up to a multiple of the system page size */
    stack_size = (stack_size + (page_size-1)) & ~(page_size-1);

    if ((res = pthread_attr_init(&attr)) != 0) {
        assert(res == 0 && "Call to pthread_attr_init failed");
        return THREAD_ID_INVALID;
    }
    if ((res = pthread_attr_setstacksize(&attr, stack_size)) != 0) {
        assert(res == 0 && "Call to pthread_attr_setstacksize failed");
        (void) pthread_attr_destroy(&attr);
        return THREAD_ID_INVALID;
    }

    tstart.main = entry_point;
    tstart.argp = argp;
    if ((res = pthread_create(&thread_id, &attr, pthread_wrapper_entry, &tstart)) != 0) {
        assert(res == 0 && "Call to pthread_create failed");
        (void) pthread_attr_destroy(&attr);
        return THREAD_ID_INVALID;
    }

    (void) pthread_attr_destroy(&attr);
    return(thread_id_t) thread_id;
}

uint32_t
thread_join
(
    thread_id_t thread_id
)
{
    if (thread_id != THREAD_ID_INVALID) {
        void *ret_val = NULL;
        int  join_res = pthread_join((pthread_t) thread_id, &ret_val);
        if (join_res == 0) { /* Thread terminated */
            return (uint32_t)(uintptr_t)(ret_val);
        } else {
            assert(join_res == 0 && "Call to pthread_join failed");
            return EXIT_FAILURE;
        }
    } else {
        return EXIT_FAILURE;
    }
}

struct job_queue_t*
job_queue_create
(
    uint32_t queue_id
)
{
    job_queue_posix_t   *queue = NULL;
    job_descriptor_t **storage = NULL;
    size_t       storage_bytes = JOB_COUNT_MAX * sizeof(job_descriptor_t*);
    int                 access = PROT_READ   | PROT_WRITE;
    int                  flags = MAP_PRIVATE | MAP_ANONYMOUS;

    if ((queue =(job_queue_posix_t*) malloc(sizeof(job_queue_posix_t))) == NULL) {
        return NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              
    }
    if ((storage = mmap(NULL, storage_bytes, access, flags, -1, 0)) != MAP_FAILED) {
        queue->storage      = storage;
        queue->push_count   = 0;
        queue->take_count   = 0;
        queue->signal       = 0;
        queue->queue_id     = queue_id;
        (void) pthread_mutex_init(&queue->mutex, NULL);
        (void) pthread_cond_init (&queue->consumer_cv, NULL);
        (void) pthread_cond_init (&queue->producer_cv, NULL);
        return (struct job_queue_t*) queue;
    } else { /* Storage allocation failed */
        free(queue);
        return NULL;
    }
}

void
job_queue_delete
(
    struct job_queue_t *queue
)
{
    if (queue != NULL) {
        job_queue_posix_t *queue_ = (job_queue_posix_t*) queue;
        (void) pthread_cond_destroy (&queue_->producer_cv);
        (void) pthread_cond_destroy (&queue_->consumer_cv);
        (void) pthread_mutex_destroy(&queue_->mutex);
        if (queue_->storage != NULL) {
            (void) munmap(queue_->storage, JOB_COUNT_MAX * sizeof(job_descriptor_t*));
            queue_->storage  = NULL;
        }
        free(queue_);
    }
}

void
job_queue_flush
(
    struct job_queue_t *queue
)
{
    job_queue_posix_t *queue_ =(job_queue_posix_t*) queue;
    if (pthread_mutex_lock(&queue_->mutex) == 0) {
        queue_->push_count = 0;
        queue_->take_count = 0;
        (void) pthread_mutex_unlock(&queue_->mutex);
        (void) pthread_cond_broadcast(&queue_->producer_cv);
    }
}

void
job_queue_signal
(
    struct job_queue_t *queue,
    uint32_t           signal
)
{
    job_queue_posix_t *queue_ =(job_queue_posix_t*) queue;
    if (pthread_mutex_lock(&queue_->mutex) == 0) {
        queue_->signal = signal;
        (void) pthread_mutex_unlock(&queue_->mutex);
        if (signal != JOB_QUEUE_SIGNAL_CLEAR) {
            (void) pthread_cond_broadcast(&queue_->consumer_cv);
            (void) pthread_cond_broadcast(&queue_->producer_cv);
        }
    }
}

uint32_t
job_queue_check_signal
(
    struct job_queue_t *queue
)
{
    job_queue_posix_t *queue_ =(job_queue_posix_t*) queue;
    uint32_t           signal = 0;

    if (pthread_mutex_lock(&queue_->mutex) == 0) {
        signal = queue_->signal;
        (void) pthread_mutex_unlock(&queue_->mutex);
    } return signal;
}

uint32_t
job_queue_get_id
(
    struct job_queue_t *queue
)
{
    return ((job_queue_posix_t*) queue)->queue_id;
}

uint32_t
job_queue_push
(
    struct job_queue_t    *queue, 
    struct job_descriptor_t *job
)
{
    if (job != NULL) {
        job_queue_posix_t  *queue_ =(job_queue_posix_t*) queue;
        job_descriptor_t **storage = queue_->storage;
        uint32_t const        mask = JOB_COUNT_MAX - 1;
        uint32_t             index;

        if (pthread_mutex_lock(&queue_->mutex) == 0) {
            for ( ; ; ) {
                if ((queue_->take_count + JOB_COUNT_MAX) > queue_->push_count || queue_->signal != JOB_QUEUE_SIGNAL_CLEAR) {
                    break;
                } else {
                    (void) pthread_cond_wait(&queue_->producer_cv, &queue_->mutex);
                }
            }
            if (queue_->signal == JOB_QUEUE_SIGNAL_CLEAR) {
                index = queue_->push_count & mask;
                storage[index] = job;
                queue_->push_count++;
                (void) pthread_mutex_unlock(&queue_->mutex);
                (void) pthread_cond_signal (&queue_->consumer_cv);
                return 1;
            } else { /* Queue is signaled */
                (void) pthread_mutex_unlock(&queue_->mutex);
                return 0;
            }
        }
    } return 0;
}

struct job_descriptor_t*
job_queue_take
(
    struct job_queue_t    *queue
)
{
    job_queue_posix_t  *queue_ =(job_queue_posix_t*) queue;
    job_descriptor_t **storage = queue_->storage;
    job_descriptor_t     *item = NULL;
    uint32_t const        mask = JOB_COUNT_MAX - 1;
    uint32_t             index;

    if (pthread_mutex_lock(&queue_->mutex) == 0) {
        for ( ; ; ) {
            if (queue_->take_count < queue_->push_count || queue_->signal != JOB_QUEUE_SIGNAL_CLEAR) {
                break;
            } else {
                (void) pthread_cond_wait(&queue_->consumer_cv, &queue_->mutex);
            }
        }
        if (queue_->signal == JOB_QUEUE_SIGNAL_CLEAR) {
            index = queue_->take_count & mask;
            item  = storage[index];
            queue_->take_count++;
            (void) pthread_mutex_unlock(&queue_->mutex);
            (void) pthread_cond_signal (&queue_->producer_cv);
            return item;
        } else { /* Queue is signaled */
            (void) pthread_mutex_unlock(&queue_->mutex);
            return NULL;
        }
    } return NULL;
}

struct job_scheduler_t*
job_scheduler_create
(
    size_t context_count
)
{
    job_scheduler_posix_t *scheduler = NULL;
    job_context_t            *jobctx = NULL;
    job_buffer_t             *jobbuf = NULL;
    uint8_t             *memory_base = NULL;
    uint8_t                     *ptr = NULL;
    size_t           jobbuf_capacity =(JOB_COUNT_MAX + (JOB_BUFFER_JOB_COUNT - 1)) / JOB_BUFFER_JOB_COUNT;
    size_t              bytes_needed = 0;
    size_t                 page_size = 0;
    size_t                         i = 0;
    long               page_size_raw = sysconf(_SC_PAGESIZE);
    int                       access = PROT_READ   | PROT_WRITE;
    int                        flags = MAP_PRIVATE | MAP_ANONYMOUS;

    if (context_count == 0) {
        context_count  = 16;
    }
    if (page_size_raw <= 0) {
        page_size   = 4096;
    } else {
        page_size   =(size_t  ) page_size_raw;
    }

    /* Determine the amount of memory required, rounded up to the next multiple of page size */
    bytes_needed     += sizeof(job_scheduler_posix_t );
    bytes_needed     += sizeof(job_descriptor_t      )  *   JOB_COUNT_MAX;    /* jobdesc     */
    bytes_needed     += sizeof(job_data_posix_t      )  *   JOB_COUNT_MAX;    /* jobdata     */
    bytes_needed     += sizeof(job_buffer_t         *)  *   jobbuf_capacity;  /* jobbuf      */
    bytes_needed      =(bytes_needed + (page_size - 1)) & ~(page_size - 1);

    /* Allocate all memory as a single large block using the VMM so it's pre-zeroed */
    if ((memory_base  =(uint8_t*) mmap(NULL, bytes_needed, access, flags, -1, 0)) == NULL) {
        return NULL;
    } ptr = memory_base;

    /* Sub-allocate various arrays from the larger block */
    scheduler          = (job_scheduler_posix_t*) ptr; ptr += sizeof(job_scheduler_posix_t);
    scheduler->jobdesc = (job_descriptor_t     *) ptr; ptr += sizeof(job_descriptor_t     ) * JOB_COUNT_MAX;
    scheduler->jobdata = (job_data_posix_t     *) ptr; ptr += sizeof(job_data_posix_t     ) * JOB_COUNT_MAX;
    scheduler->jobbuf  = (job_buffer_t        **) ptr; ptr += sizeof(job_buffer_t        *) * jobbuf_capacity;

    /* Pre-allocate the specified number of job contexts */
    for (i = 0; i < context_count; ++i) {
        if ((jobctx = (job_context_t*) malloc(sizeof(job_context_t))) == NULL) {
            goto cleanup_and_fail;
        } else {
            jobctx->next   = scheduler->jobctx_flist;
            jobctx->jobbuf = NULL;
            jobctx->queue  = NULL;
            jobctx->sched  =(struct job_scheduler_t*) scheduler;
            jobctx->thrid  = THREAD_ID_INVALID;
            jobctx->user1  = 0;
            jobctx->user2  = 0;
            jobctx->jobcnt = 0;
            jobctx->pad1   = 0;
            scheduler->jobctx_flist = jobctx;
        }
    }

    /* Pre-allocate enough job buffers for each context */
    for (i = 0; i < context_count; ++i) {
        if ((jobbuf = job_buffer_create(i)) == NULL) {
            goto cleanup_and_fail;
        } else {
            jobbuf->next = scheduler->jobbuf_flist;
            scheduler->jobbuf_flist = jobbuf;
            scheduler->jobbuf[i]    = jobbuf;
        }
    }

    (void) pthread_rwlock_init(&scheduler->queue_rwlock , NULL);
    (void) pthread_rwlock_init(&scheduler->jobctx_rwlock, NULL);
    (void) pthread_mutex_init (&scheduler->jobbuf_mutex , NULL);
    scheduler->queue_count  = 0;
    scheduler->memsize      = bytes_needed;
    scheduler->jobctx_count = context_count;
    scheduler->jobbuf_limit = jobbuf_capacity;
    scheduler->jobbuf_count = context_count;
    return (struct job_scheduler_t*) scheduler;

cleanup_and_fail:
    if (memory_base != NULL) {
        if (scheduler != NULL) {
            while (scheduler->jobbuf_flist != NULL) {
                jobbuf = scheduler->jobbuf_flist;
                scheduler->jobbuf_flist = jobbuf->next;
                job_buffer_delete(jobbuf);
            }
            while (scheduler->jobctx_flist != NULL) {
                jobctx = scheduler->jobctx_flist;
                scheduler->jobctx_flist = jobctx->next;
                free(jobctx);
            }
        }
        (void) munmap((void*) memory_base, bytes_needed);
    }
    return NULL;
}

void
job_scheduler_delete
(
    struct job_scheduler_t *scheduler
)
{
    if (scheduler != NULL) {
        job_scheduler_posix_t *sched_ =(job_scheduler_posix_t*) scheduler;
        job_context_t        *jobctx  = NULL;
        size_t               ctxfree  = 0;
        size_t                  i, n;

        for (i = 0; i < JOB_COUNT_MAX; ++i) {
            if (sched_->jobdata[i].state != JOB_STATE_UNINITIALIZED) {
                (void) pthread_mutex_destroy(&sched_->jobdata[i].lock);
                sched_->jobdata[i].state  = JOB_STATE_UNINITIALIZED;
            }
        }
        if (pthread_mutex_lock(&sched_->jobbuf_mutex) == 0) {
            for (i = 0, n = sched_->jobbuf_count; i < n; ++i) {
                job_buffer_delete(sched_->jobbuf[i]);
                sched_->jobbuf[i] = NULL;
            }
            (void) pthread_mutex_unlock(&sched_->jobbuf_mutex);
        }
        if (pthread_rwlock_wrlock(&sched_->jobctx_rwlock) == 0) {
            while (sched_->jobctx_flist != NULL) {
                jobctx = sched_->jobctx_flist;
                sched_->jobctx_flist = jobctx->next;
                free(jobctx); // NOTE: ctx->jobbuf was freed above
                ctxfree++;
            }
            (void) pthread_rwlock_unlock(&sched_->jobctx_rwlock);
            assert(ctxfree == sched_->jobctx_count && "One or more non-released job contexts detected, these will leak");
        }
        (void) pthread_mutex_destroy (&sched_->jobbuf_mutex);
        (void) pthread_rwlock_destroy(&sched_->jobctx_rwlock);
        (void) pthread_rwlock_destroy(&sched_->queue_rwlock);
        (void) munmap((void*) sched_, (size_t) sched_->memsize);
    }
}

void
job_scheduler_terminate
(
    struct job_scheduler_t *scheduler
)
{
    if (scheduler != NULL) {
        job_scheduler_posix_t *sched_ =(job_scheduler_posix_t*) scheduler;
        size_t                  i, n;

        if (pthread_rwlock_rdlock(&sched_->queue_rwlock) == 0) {
            for (i = 0, n = sched_->queue_count; i < n; ++i) {
                struct job_queue_t *queue = sched_->job_queues[i];
                job_queue_signal(queue, JOB_QUEUE_SIGNAL_TERMINATE);
            } (void) pthread_rwlock_unlock(&sched_->queue_rwlock);
        }
    }
}

struct job_context_t*
job_scheduler_acquire_context
(
    struct job_scheduler_t *scheduler,
    struct job_queue_t    *wait_queue,
    thread_id_t             owner_tid
)
{
    if (scheduler != NULL) {
        job_scheduler_posix_t *sched_ =(job_scheduler_posix_t*) scheduler;
        job_context_t           *ctx  = NULL;
        job_buffer_t            *buf  = NULL;
        uint32_t                 qid  = job_queue_get_id(wait_queue);
        uint32_t                 qix  = JOB_QUEUE_COUNT_MAX;
        size_t                  i, n;

        if (pthread_rwlock_wrlock(&sched_->jobctx_rwlock) == 0) {
            /* Acquire or allocate a job_context_t */
            if ((ctx = sched_->jobctx_flist) != NULL) {
                sched_->jobctx_flist = ctx->next;
            } else if ((ctx = (job_context_t*) malloc(sizeof(job_context_t))) != NULL) {
                sched_->jobctx_count++;
            } /* Else, failed to allocate a context */
            (void) pthread_rwlock_unlock(&sched_->jobctx_rwlock);

            /* Acquire or allocate a job_buffer_t to assign to the context */
            if (ctx != NULL && (buf = jobbuf_acquire(sched_, NULL)) != NULL) {
                ctx->next   = NULL;
                ctx->jobbuf = buf;
                ctx->queue  =(struct job_queue_t    *) wait_queue;
                ctx->sched  =(struct job_scheduler_t*) scheduler;
                ctx->thrid  = owner_tid;
                ctx->user1  = 0;
                ctx->user2  = 0;
                ctx->jobcnt = 0;
                ctx->pad1   = 0;
            } else { /* Failed to acquire a job buffer. */
                if (ctx != NULL) { /* Return ctx to the free pool. */
                    ctx->next  = sched_->jobctx_flist;
                    sched_->jobctx_flist = ctx;
                    ctx  = NULL;
                }
            }

            /* Store queue reference & update queue reference count */
            if (ctx != NULL && pthread_rwlock_wrlock(&sched_->queue_rwlock) == 0) {
                for (i = 0, n = sched_->queue_count; i < n; ++i) {
                    if (sched_->queue_refs[i] == qid) {
                        qix = (uint32_t) i;
                        break;
                    }
                }
                if (qix == JOB_QUEUE_COUNT_MAX) { /* Unknown queue */
                    sched_->job_queues[sched_->queue_count] = wait_queue;
                    sched_->queue_refs[sched_->queue_count] = 1;
                    sched_->queue_ids [sched_->queue_count] = qid;
                    sched_->queue_count++;
                } else { /* Known queue, bump reference count */
                    sched_->queue_refs[qix]++;
                }
                (void) pthread_rwlock_unlock(&sched_->queue_rwlock);
            }
        } return ctx;
    } return NULL;
}

void
job_scheduler_release_context
(
    struct job_scheduler_t *scheduler,
    struct job_context_t     *context
)
{
    if (scheduler != NULL && context != NULL) {
        job_scheduler_posix_t *sched_ =(job_scheduler_posix_t*) scheduler;
        job_buffer_t            *buf  = context->jobbuf;
        uint32_t                 qid  = job_queue_get_id(context->queue);
        uint32_t                 qix  = JOB_QUEUE_COUNT_MAX;
        size_t                  i, n;

        // Return the context back to the free pool.
        if (pthread_rwlock_wrlock(&sched_->jobctx_rwlock) == 0) {
            context->next        = sched_->jobctx_flist;
            context->jobbuf      = NULL;
            context->queue       = NULL;
            context->thrid       = THREAD_ID_INVALID;
            sched_->jobctx_flist = context;
            (void) pthread_rwlock_unlock(&sched_->jobctx_rwlock);
        }

        // Return the job buffer back to the free pool, if possible.
        jobbuf_release(sched_, buf);

        // Decrement the reference count on the queue.
        if (pthread_rwlock_wrlock(&sched_->queue_rwlock) == 0) {
            for (i = 0, n = sched_->queue_count; i < n; ++i) {
                if (sched_->queue_ids[i] == qid) {
                    qix  = (uint32_t) i;
                    break;
                }
            }
            if (qix != JOB_QUEUE_COUNT_MAX) {
                sched_->queue_refs[qix]--;
                if (sched_->queue_refs[qix] == 0) {
                    /* No outstanding references */
                    sched_->job_queues[qix]  = sched_->job_queues[sched_->queue_count-1];
                    sched_->queue_refs[qix]  = sched_->queue_refs[sched_->queue_count-1];
                    sched_->queue_ids [qix]  = sched_->queue_ids [sched_->queue_count-1];
                    sched_->queue_count--;
                }
            } (void) pthread_rwlock_unlock(&sched_->queue_rwlock);
        }
    }
}

struct job_queue_t*
job_scheduler_get_queue
(
    struct job_scheduler_t *scheduler,
    uint32_t                 queue_id
)
{
    if (scheduler != NULL) {
        job_scheduler_posix_t *sched_ =(job_scheduler_posix_t*) scheduler;
        struct job_queue_t    *queue  = NULL;
        size_t                  i, n;

        if (pthread_rwlock_rdlock(&sched_->queue_rwlock) == 0) {
            for (i = 0, n = sched_->queue_count; i < n; ++i) {
                if (sched_->queue_ids[i] == queue_id) {
                    queue = sched_->job_queues[i];
                    break;
                }
            } (void) pthread_rwlock_unlock(&sched_->queue_rwlock);
        } return queue;
    } return NULL;
}

uint32_t
job_scheduler_get_queue_worker_count
(
    struct job_scheduler_t *scheduler,
    uint32_t                 queue_id
)
{
    if (scheduler != NULL) {
        job_scheduler_posix_t *sched_ =(job_scheduler_posix_t*) scheduler;
        uint32_t                refs  = 0;
        size_t                  i, n;

        if (pthread_rwlock_rdlock(&sched_->queue_rwlock) == 0) {
            for (i = 0, n = sched_->queue_count; i < n; ++i) {
                if (sched_->queue_ids[i] == queue_id) {
                    refs = sched_->queue_refs[i];
                    break;
                }
            } (void) pthread_rwlock_unlock(&sched_->queue_rwlock);
        } return refs;
    } return 0;
}

int
job_scheduler_cancel
(
    struct job_scheduler_t *scheduler,
    job_id_t                       id
)
{
    if (job_id_valid(id)) {
        job_scheduler_posix_t  *sched_=(job_scheduler_posix_t  *) scheduler;
        uint32_t           slot_index = job_id_get_slot_index_u32(id);
        job_data_posix_t    *job_data =&sched_->jobdata[slot_index];
        job_state_e             state = JOB_STATE_UNINITIALIZED;
        if (pthread_mutex_lock(&job_data->lock) == 0) {
            if (state != JOB_STATE_RUNNING && state != JOB_STATE_COMPLETED) {
                state  = job_data->state = JOB_STATE_CANCELED;
            } else {
                state  = job_data->state;
            } (void) pthread_mutex_unlock(&job_data->lock);
        } return state;
    } else {
        return JOB_STATE_UNINITIALIZED;
    }
}

struct job_scheduler_t*
job_context_scheduler
(
    struct job_context_t   *context
)
{
    if (context != NULL) {
        return context->sched;
    } else {
        return NULL;
    }
}

struct job_queue_t*
job_context_queue
(
    struct job_context_t   *context
)
{
    if (context != NULL) {
        return context->queue;
    } else {
        return NULL;
    }
}

thread_id_t
job_context_thread_id
(
    struct job_context_t   *context
)
{
    if (context != NULL) {
        return context->thrid;
    } else {
        return THREAD_ID_INVALID;
    }
}

struct job_descriptor_t*
job_context_create_job
(
    struct job_context_t   *context,
    size_t                data_size,
    size_t               data_align
)
{
    job_scheduler_posix_t *sched =(job_scheduler_posix_t*) context->sched;
    job_descriptor_t        *job = NULL;
    job_data_posix_t       *data = NULL;
    job_buffer_t         *jobbuf = context->jobbuf;
    uint32_t           new_count = context->jobcnt + 1;
    uint32_t          slot_index = 0;
    size_t           jobbuf_mark = jobbuf ->nxtofs;
    uint16_t          *wait_list = NULL;
    uint8_t           *user_data = NULL;
    size_t const       wait_size = JOB_WAITER_COUNT_MAX  * sizeof(uint16_t);
    size_t const    maximum_size = JOB_BUFFER_SIZE_BYTES - wait_size;
    uint32_t          generation = 0;

    if (data_size > maximum_size) {
        assert(data_size <= maximum_size && "Requested job data size exceeds maximum, increase JOB_BUFFER_SIZE_BYTES");
        return NULL;
    }
    for ( ; ; ) {
        /* Allocate the wait list and user data separately */
        if ((wait_list = (uint16_t*) jobbuf_alloc(jobbuf, wait_size, SCHEDULER_alignof_type(uint16_t))) == NULL) {
            goto allocation_failed;
        }
        if (data_size != 0) {
            if ((user_data = (uint8_t *) jobbuf_alloc(jobbuf, data_size, data_align)) == NULL) {
                goto allocation_failed;
            }
        }
        /* Determine the slot allocated to the job */
        slot_index = context->jobcnt + jobbuf->jobbase;
        break;

    allocation_failed:
        jobbuf ->nxtofs = jobbuf_mark;
        context->jobbuf = jobbuf_acquire(sched, context->jobbuf);
        context->jobcnt = 0;
        jobbuf          = context->jobbuf;
        jobbuf_mark     = jobbuf ->nxtofs;
        assert(context->jobbuf != NULL && "Failed to acquire new job buffer (allocation)");
        continue;
    }

    /* Take a reference on the job buffer */
    atomic_increment_u32(&jobbuf->refcnt);

    /* Initialize the job descriptor (public data) */
    job  = &sched->jobdesc[slot_index];
    data = &sched->jobdata[slot_index];
    generation   = job_id_get_generation_u32(job->id) + 1;
    job->jobbuf  = jobbuf;
    job->target  = NULL;
    job->jobmain = NULL;
    job->user1   = 0;
    job->user2   = 0;
    job->data    = user_data;
    job->size    =(uint32_t) data_size;
    job->id      = job_id_pack(slot_index, generation);
    job->parent  = JOB_ID_INVALID;
    job->exit    = 0;

    /* Initialize the internal per-job state */
    if (data->state == JOB_STATE_UNINITIALIZED) {
        (void) pthread_mutex_init(&data->lock, NULL);
    }
    data->waiters = wait_list;
    data->waitcnt = 0;
    data->wait    =-1; /* Not ready-to-run until submitted */
    data->work    = 1; /* One work item representing self  */
    data->state   = JOB_STATE_NOT_SUBMITTED;

    /* Update state on the job context */
    if (new_count != JOB_BUFFER_JOB_COUNT) {
        context->jobcnt = new_count;
    } else {
        context->jobbuf = jobbuf_acquire(sched, context->jobbuf);
        context->jobcnt = 0;
        assert(context->jobbuf != NULL && "Failed to acquire new job buffer (job count)");
    }
    return job;
}

int
job_context_submit_job
(
    struct job_context_t   *context,
    struct job_descriptor_t    *job,
    job_id_t const *dependency_list,
    size_t         dependency_count,
    int                 submit_type
)
{
    if (job != NULL) {
        struct job_queue_t  *defaultq = context->queue;
        job_scheduler_posix_t  *sched =(job_scheduler_posix_t*) context->sched;
        job_data_posix_t    *all_data = sched->jobdata;
        uint32_t             job_slot = job_id_get_slot_index_u32(job->id);
        uint32_t          parent_slot = job_id_get_slot_index_u32(job->parent);
        job_data_posix_t    *dep_data = NULL;
        job_data_posix_t    *job_data =&sched->jobdata[job_slot];
        job_data_posix_t *parent_data =&sched->jobdata[parent_slot];
        job_state_e             state = JOB_STATE_NOT_SUBMITTED;
        int32_t            wait_count = 0;
        job_submit_result_e    result = JOB_SUBMIT_SUCCESS;
        uint32_t            dep_slot;
        size_t                     i;

        if (job->target  == NULL) {
            job->target   = defaultq;
        }
        if (job->jobmain == NULL) {
            job->jobmain  = default_job_main;
        }

        /* Determine the initial state of the job, which will be one of:
         * - JOB_STATE_NOT_READY: The job has a non-zero dependency count and one or more dependencies are not completed.
         * - JOB_STATE_READY    : The job has no dependencies, or all dependencies have completed.
         * - JOB_STATE_CANCELED : The submit_type is JOB_SUBMIT_CANCEL.
         * If submit_type is JOB_SUBMIT_RUN, the job cancelation status is determined when it is pulled from the ready-to-run queue.
         * This avoids the need to backtrack and un-register waiters, etc.
         */

        if (submit_type == JOB_SUBMIT_RUN) {
            /* Register the job as a waiter on all non-completed dependencies.
             * Note that dependencies may complete before this job is submitted,
             * and once this job has registered on their waiter list, they will
             * attempt to modify the job's wait field - allow this, but do NOT 
             * allow the job to be made ready-to-run. 
             */
            for (i = 0; i < dependency_count; ++i) {
                if (job_id_valid(dependency_list[i])) {
                    dep_slot = job_id_get_slot_index_u32(dependency_list[i]);
                    dep_data =&all_data[dep_slot];
                    if (pthread_mutex_lock(&dep_data->lock) == 0) {
                        if (dep_data->state != JOB_STATE_COMPLETED && dep_data->state != JOB_STATE_CANCELED) {
                            if (dep_data->waitcnt != JOB_WAITER_COUNT_MAX) {
                                dep_data->waiters[dep_data->waitcnt++] = (uint16_t) job_slot;
                                wait_count++;
                            } else {
                                assert(dep_data->waitcnt < JOB_WAITER_COUNT_MAX);
                                result = JOB_SUBMIT_TOO_MANY_WAITERS;
                            }
                        } (void) pthread_mutex_unlock(&dep_data->lock);
                    }
                }
            }
            if (wait_count == 0) {
                state = JOB_STATE_READY;
            } else {
                state = JOB_STATE_NOT_READY;
            }
            /* If the job has a parent job, register the job as an outstanding 
             * work item with the parent task. This prevents the parent from 
             * completing before any of its child jobs complete.
             */
            if (job_id_valid(job->parent)) {
                parent_slot = job_id_get_slot_index_u32(job->parent);
                parent_data =&all_data[parent_slot];
                if (pthread_mutex_lock(&parent_data->lock) == 0) {
                    if (parent_data->state != JOB_STATE_CANCELED) {
                        parent_data->work++;
                    } (void) pthread_mutex_unlock(&parent_data->lock);
                }
            }
        }
        if (submit_type == JOB_SUBMIT_CANCEL) {
            state = JOB_STATE_CANCELED;
        }

        /* Set up internal job data.
         * This performs the necessary book keeping to handle completed dependencies.
         * The +1 added to the wait count is to counteract the -1 initial value from 
         * when the job was created, and signals the end of the submission operation.
         * The wait count may be less than -1 if one or more dependencies have 
         * completed in between registering the job as a waiter and submission completion.
         */
        if (pthread_mutex_lock(&job_data->lock) == 0) {
            if((job_data->wait   =(job_data->wait + wait_count + 1)) == 0) {
                state = JOB_STATE_READY;
            }
            if (job_data->state != JOB_STATE_CANCELED) {
                job_data->state  = state;
            } (void) pthread_mutex_unlock(&job_data->lock);
        }

        /* Finally, potentially submit the job to the ready-to-run queue. */
        if (state != JOB_STATE_NOT_READY) {
            job_queue_push(job->target, job);
        } return result;
    } else {
        return JOB_SUBMIT_INVALID_JOB;
    }
}

int
job_context_cancel_job
(
    struct job_context_t   *context,
    job_id_t                     id
)
{
    return job_scheduler_cancel(context->sched, id);
}

int
job_context_wait_job
(
    struct job_context_t *context,
    job_id_t                   id
)
{
    job_scheduler_posix_t *sched =(job_scheduler_posix_t*) context->sched;
    job_descriptor_t   *wait_job = NULL;
    job_descriptor_t   *exec_job = NULL;
    job_data_posix_t  *wait_data = NULL;
    int32_t           wait_state = JOB_STATE_UNINITIALIZED;
    uint32_t           wait_slot = 0;

    if (job_id_valid(id)) {
        wait_slot = job_id_get_slot_index_u32(id);
        wait_job  =&sched->jobdesc[wait_slot];
        wait_data =&sched->jobdata[wait_slot];
        if (wait_job->id != id) {
            /* The generation has changed, the job has already completed */
            return 1;
        }
        for ( ; ; ) {
            /* Retrieve the current state of the job we're waiting for */
            if (pthread_mutex_lock(&wait_data->lock) == 0) {
                wait_state = wait_data->state;
                (void) pthread_mutex_unlock(&wait_data->lock);
            }
            if (wait_state == JOB_STATE_COMPLETED || wait_state == JOB_STATE_CANCELED) {
                return 1;
            }
            /* The job hasn't completed yet, so take and execute a job */
            if ((exec_job = job_context_wait_ready_job(context)) != NULL) {
                exec_job->exit = exec_job->jobmain(context, exec_job);
                job_context_complete_job(context, exec_job);
            } else {
                return 0; /* Queue was signaled */
            }
        }
    } return 0;
}

struct job_descriptor_t*
job_context_wait_ready_job
(
    struct job_context_t *context
)
{
    job_scheduler_posix_t  *sched =(job_scheduler_posix_t*) context->sched;
    job_descriptor_t    *itr_desc = NULL;
    job_descriptor_t    *job_desc = NULL;
    job_data_posix_t    *job_data = NULL;
    job_data_posix_t    *itr_data = NULL;
    job_id_t               itr_id = JOB_ID_INVALID;
    uint32_t             canceled = 0;
    uint32_t             itr_slot;

    for ( ; ; ) {
        if ((job_desc = job_queue_take(context->queue)) == NULL) {
            return NULL; /* Queue was signaled, abort the wait */
        }

        canceled = 0;
        itr_id   = job_desc->id;
        itr_slot = job_id_get_slot_index_u32(job_desc->id);
        itr_data =&sched->jobdata[itr_slot];
        itr_desc = job_desc;
        job_data = itr_data;
        do { /* Determine job cancelation status */
            if (pthread_mutex_lock(&itr_data->lock) == 0) {
                if (itr_data->state == JOB_STATE_CANCELED) {
                    canceled = 1;
                } (void) pthread_mutex_unlock(&itr_data->lock);
            }
            itr_id   = itr_desc->parent;
            itr_slot = job_id_get_slot_index_u32(itr_desc->parent);
            itr_desc =&sched->jobdesc[itr_slot];
            itr_data =&sched->jobdata[itr_slot];
        } while (canceled == 0 && job_id_valid(itr_id));

        if (canceled == 0) {
            /* This is a non-canceled, ready-to-run job */
            if (pthread_mutex_lock(&job_data->lock) == 0) {
                job_data->state = JOB_STATE_RUNNING;
                (void) pthread_mutex_unlock(&job_data->lock);
            } return job_desc;
        } else {
            /* Complete the job without returning to the caller */
            if (job_data->state != JOB_STATE_CANCELED) {
                if (pthread_mutex_lock(&job_data->lock) == 0) {
                    job_data->state = JOB_STATE_CANCELED;
                    (void) pthread_mutex_unlock(&job_data->lock);
                }
            } job_context_complete_job(context, job_desc);
        }
    }
}

void
job_context_complete_job
(
    struct job_context_t   *context,
    struct job_descriptor_t    *job
)
{
    job_scheduler_posix_t  *sched =(job_scheduler_posix_t*) context->sched;
    job_descriptor_t    *all_jobs = sched->jobdesc;
    job_data_posix_t    *all_data = sched->jobdata;
    job_data_posix_t    *job_data = NULL;
    job_data_posix_t   *wait_data = NULL;
    job_descriptor_t    *wait_job = NULL;
    job_descriptor_t  *parent_job = NULL;
    uint32_t             job_slot = job_id_get_slot_index_u32(job->id);
    job_id_t            parent_id = job->parent;
    uint32_t            completed = 0;
    uint32_t           wait_count = 0;
    uint32_t            job_ready = 0;
    uint16_t            wait_list[JOB_WAITER_COUNT_MAX];
    uint32_t            wait_slot;
    uint32_t          parent_slot;
    uint32_t                    i;

    job_data = &all_data[job_slot];
    if (pthread_mutex_lock(&job_data->lock) == 0) {
        if (job_data->work-- == 1) {
            /* The job has actually completed - copy the waiter list */
            memcpy(wait_list, job_data->waiters, job_data->waitcnt * sizeof(uint16_t));
            wait_count = job_data->waitcnt;
            completed  = 1; /* Also applies to canceled jobs */
            if (job_data->state != JOB_STATE_CANCELED) {
                job_data->state  = JOB_STATE_COMPLETED;
            }
        } (void) pthread_mutex_unlock(&job_data->lock);
    }
    if (completed) {
        /* Release the reference on the job buffer */
        jobbuf_release(sched, job->jobbuf);

        /* Update and possibly release jobs waiting on this job */
        for (i = 0; i < wait_count; ++i) {
            job_ready = 0;
            wait_slot = wait_list[i];
            wait_data = &all_data[wait_slot];
            wait_job  = &all_jobs[wait_slot];
            if (pthread_mutex_lock(&wait_data->lock) == 0) {
                if (wait_data->wait-- == 1) {
                    if (wait_data->state != JOB_STATE_CANCELED) {
                        wait_data->state  = JOB_STATE_READY;
                    } job_ready = 1;
                } (void) pthread_mutex_unlock(&wait_data->lock);
            }
            if (job_ready) {
                job_queue_push(wait_job->target, wait_job);
            }
        }
    }
    if (job_id_valid(parent_id)) { /* Complete the parent job */
        parent_slot = job_id_get_slot_index_u32(parent_id);
        parent_job  =&sched->jobdesc[parent_slot];
        job_context_complete_job(context, parent_job);
    }
}
