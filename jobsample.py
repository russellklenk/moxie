#!/usr/bin/env python
import sys
import random

from   threading import Event
from   time      import sleep

from   moxie.scheduler import JobQueue
from   moxie.scheduler import JobSystem
from   moxie.scheduler import JobContext
from   moxie.scheduler import JobSubmitType
from   moxie.scheduler import JobSubmitResult
from   moxie.scheduler import JobSystemThread

"""
Provides a simple example showing how to initialize a `JobSystem`, submit some work, and wait for it to complete.
"""

def print_job(job: int, jobctx: JobContext, name: str) -> int:
    """
    Print a simple message to stdout and return.

    Parameters
    ----------
        job   : The identifier of the executing job.
        jobctx: The `JobContext` used to interact with the `JobSystem` from this thread.
        name  : A user-provided name for the job.

    Returns
    -------
        Zero on success, or non-zero otherwise.
    """
    print(f'Running print_job {name} on thread {jobctx.thread_id}.')
    sleep(random.random()) # Sleep between 0 and 1 seconds
    return 0

def spawn_work_job(job: int, jobctx: JobContext) -> int:
    """
    Spawn some child jobs. The parent job `job` runs, but does not complete, until all of its child jobs have completed.
    Child jobs 'B' and 'C' can run in parallel with each other, and may also run in parallel with their parent job `job`.
    Child job 'A' cannot start running until 'B' and 'C' have completed.
    The parent job `job` cannot complete (and thus cannot allow dependent jobs to start) until all three child jobs 'A', 'B', 'C' have completed.

    Parameters
    ----------
        job   : The identifier of the executing job.
        jobctx: The `JobContext` used to interact with the `JobSystem` from this thread.

    Returns
    -------
        Zero on success, or non-zero otherwise.
    """
    print(f'Running spawn_work_job on thread {jobctx.thread_id}.')
    a: int = jobctx.create_job(callable=print_job, parent=job, name='A')
    b: int = jobctx.create_job(callable=print_job, parent=job, name='B')
    c: int = jobctx.create_job(callable=print_job, parent=job, name='C')
    # B and C must complete before A can run, but B and C can run in parallel.
    res_a: JobSubmitResult = jobctx.submit_job(a, JobSubmitType.RUN, dependencies=[b, c])
    res_b: JobSubmitResult = jobctx.submit_job(b, JobSubmitType.RUN)
    res_c: JobSubmitResult = jobctx.submit_job(c, JobSubmitType.RUN)
    assert res_a == JobSubmitResult.SUCCESS
    assert res_b == JobSubmitResult.SUCCESS
    assert res_c == JobSubmitResult.SUCCESS
    return 0

def end_of_pipe_job(job: int, jobctx: JobContext, signal: Event) -> int:
    """
    Indicate that the top-level work item has completed and signal the main thread, which is waiting on the `Event` `signal` to become signaled.

    Parameters
    ----------
        job   : The identifier of the executing job.
        jobctx: The `JobContext` used to interact with the `JobSystem` from this thread.
        signal: The `Event` being waited on to indicate that the top-level work item has completed.

    Returns
    -------
        Zero on success, or non-zero otherwise.
    """
    print(f'Running end_of_pipe_job on thread {jobctx.thread_id}.')
    signal.set()
    return 0

if __name__ == "__main__":
    # Initialization of the JobSystem with some worker threads.
    sigeop   = Event()
    system   = JobSystem()
    queue    = system.get_queue(queue_id=1) # Creates the queue with ID=1, or returns the existing queue with ID=1
    worker1  = JobSystemThread(name='Worker 1', wait_queue=queue, owner=system)
    worker2  = JobSystemThread(name='Worker 2', wait_queue=queue, owner=system)
    system.launch_threads()
    # At this point, worker1 and worker2 are parked on queue, waiting for some work to execute.

    # Acquire a JobContext that can be used to submit work to the JobSystem.
    # The JobContext should only ever be accessed by a single thread.
    # Each worker thread should acquire its own JobContext object.
    with system.acquire_context(queue) as context:
        spawn_work   : int = context.create_job(callable=spawn_work_job)
        work_complete: int = context.create_job(callable=end_of_pipe_job, signal=sigeop)
        # Submit the jobs. The spawn_work job can begin executing immediately.
        # The work_complete job cannot start to run until the spawn_work job has completed.
        context.submit_job(spawn_work   , JobSubmitType.RUN)
        context.submit_job(work_complete, JobSubmitType.RUN, dependencies=[spawn_work])
        # Block on the main thread, waiting for the work item to finish.
        # Don't use context.wait_for_job on the main thread - the application
        #  can end up deadlocked, waiting on the job queue while all of the 
        #  worker threads are also blocked waiting on the job queue.
        sigeop.wait()

    # Perform cleanup of the JobSystem.
    exit_code = JobSystemThread.EXIT_SUCCESS
    system.terminate_threads()
    thread_list = system.unregister_all_threads()
    for thread in thread_list: # In this case, these are all JobSystemThread, which have exit_code and exit_message fields.
        print(f'Thread {thread.ident} ({thread.name}) exited with code {thread.exit_code}: {thread.exit_message}.')
        if exit_code != JobSystemThread.EXIT_SUCCESS:
            exit_code = thread.exit_code

    sys.exit(exit_code)
