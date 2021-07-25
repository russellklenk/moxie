"""
Implements the high-level interface to the job scheduling system built on top of the low-level _moxie_core scheduler.
"""
import sys
import threading

from   enum      import IntEnum
from   typing    import Any, Callable, Dict, List, Optional, Tuple
from   threading import Lock

import moxie._moxie_core as _mc


class JobQueueSignal(IntEnum):
    """
    Define well-known signal values that may be raised on and received from a job queue.

    Fields
    ------
        NONE     : No signal is raised on the queue.
        CLEAR    : The current signal value should be cleared.
        TERMINATE: The job queue is being instructed that the job system is shutting down.
        USER     : The first job queue signal identifier value available to user applications.
    """
    NONE     : int = 0
    CLEAR    : int = 0
    TERMINATE: int = 1
    USER     : int = 2


class JobId(IntEnum):
    """
    Define some well-known job identifier values.

    Fields
    ------
        NONE   : A 'null' job identifier indicating 'no job'.
        INVALID: An invalid job identifier, indicating 'failure'.
    """
    NONE   : int = 0
    INVALID: int = 0


class JobState(IntEnum):
    """
    Define the states an individual job may be in at any moment in time.

    Fields
    ------
        UNINITIALIZED: The job data hasn't been initialized yet; typically the job is invalid.
        NOT_SUBMITTED: The job has been created but not yet submitted.
        NOT_READY    : The job has been submitted to run, but one or more dependencies haven't yet completed.
        READY        : The job is ready-to-run and is waiting in a job queue.
        RUNNING      : The job is actively running on a job within the job system.
        COMPLETED    : The job has finished execution.
        CANCELED     : The job was canceled before it started executing.
    """
    UNINITIALIZED: int = 0
    NOT_SUBMITTED: int = 1
    NOT_READY    : int = 2
    READY        : int = 3
    RUNNING      : int = 4
    COMPLETED    : int = 5
    CANCELED     : int = 6


class JobCallType(IntEnum):
    """
    Define the modes in which a job entry point may be called.
    A job entry point may be called multiple times for a single job to perform various phases of job execution.

    Fields
    ------
        EXECUTE: The job entry point should execute the "work" to be performed.
        CLEANUP: The job entry point should perform any resource cleanup.
    """
    EXECUTE: int = 0
    CLEANUP: int = 1


class JobSubmitType(IntEnum):
    """
    Define the job submission types. When a job is created, it must always be submitted - even if it is to be immediately canceled.

    Fields
    ------
        RUN   : The job should be submitted and execute normally.
        CANCEL: The job should be submitted and immediately canceled - the job will never run.
    """
    RUN   : int = 0
    CANCEL: int =-1


class JobSubmitResult(IntEnum):
    """
    Define the result codes returned by a job submission operation.

    Fields
    ------
        SUCCESS         : The job was submitted successfully.
        INVALID_JOB     : The job will not run, because the job identifier was invalid or one or more required fields were not specified.
        TOO_MANY_WAITERS: The job will not run, because one of the jobs it depends on has too many other jobs waiting on it.
    """
    SUCCESS         : int =  0
    INVALID_JOB     : int = -1
    TOO_MANY_WAITERS: int = -2


class JobQueue:
    """
    A waitable job queue to which jobs are submitted, and worker threads can wait on.

    Fields
    ------
        id: A unique `int` identifier for the queue within the job system.
    """
    def __init__(self, queue_id: int) -> None:
        self.id        = queue_id
        self._internal = _mc.create_job_queue(queue_id)

    def __str__(self) -> str:
        return f'JobQueue(id={self.id})'

    def __repr__(self) -> str:
        return f'JobQueue(id={self.id})'

    def signal(self, signal_id: int) -> None:
        """
        Send a signal to all threads that wait for work on the queue.
        Any parked threads will wake up and should call `JobQueue.get_signal` to retrieve the signal value specified by `signal_id`.
        No threads will be able to part on the queue until the signal has been cleared, and no work items will be returned.

        Parameters
        ----------
            signal_id: An integer value with application-defined meaning representing the signal to send.
        """
        _mc.signal_job_queue(self._internal, signal_id)

    def get_signal(self) -> int:
        """
        Retrieve the current value of the signal on the queue.

        Returns
        -------
            An `int` signal value, which may be a value with application-defined meaning, `JobQueueSignal.NONE` if there is no active signal, or `JobQueueSignal.TERMINATE` if all threads should terminate.
        """
        return _mc.check_job_queue_signal(self._internal)

    def clear_signal(self) -> None:
        """
        Clear any signal value currently set on the queue, allowing worker threads to wait on the queue again.
        """
        _mc.signal_job_queue(self._internal, JobQueueSignal.CLEAR)

    def flush(self) -> None:
        """
        Remove all pending work items from the job queue.
        """
        _mc.flush_job_queue(self._internal)


class JobSystem:
    """
    Manages resources associated with a job system and provides high-level control operations.

    Fields
    ------
        name    : A `str` specifying a name for the job system, used for debugging.
        queues  : A `list` of `JobQueue` instances representing the set of waitable queues to which work can be submitted.
        threads : A `list` of `JobSystemThread` (or subclasses thereof) representing the pool of threads that create, submit, execute and/or complete work items.
        contexts: A `list` of `JobContext` representing the set of live job management contexts used to interact with the job system from individual threads.
    """
    def __init__(self, name: str='Main', context_count: int=1) -> None:
        if context_count < 0:
            raise ValueError(f'The supplied context count {context_count} must be >= 0')

        self.name              : str    = name if name is not None else 'Unnamed'
        self.queues            : list   = [] # JobQueue
        self.threads           : list   = [] # JobSystemThread
        self.contexts          : list   = [] # JobContext
        self._internal         : object = _mc.create_job_scheduler(context_count)
        self._id_to_context    : dict   = {} # int -> JobContext
        self._id_to_queue      : dict   = {} # int -> JobQueue
        self._id_to_thread     : dict   = {} # int -> JobSystemThread
        self._id_to_thread_name: dict   = {} # int -> str
        self._queue_list_lock  : Lock   = Lock()
        self._thread_list_lock : Lock   = Lock()
        self._context_list_lock: Lock   = Lock()

    def __str__(self) -> str:
        return f'JobQueue(name={self.name})'

    def __repr__(self) -> str:
        return f'JobQueue(name={self.name})'

    def get_queue(self, queue_id: int) -> JobQueue:
        """
        Retrieve an existing `JobQueue`, or create a new job queue and register it with the `JobSystem` if necessary.
        Queues should be created from the main thread at startup, prior to submitting any work.

        Parameters
        ----------
            queue_id: An application-defined integer uniquely identifying the queue within the `JobSystem`.

        Returns
        -------
            A new `JobQueue` instance.
        """
        with self._queue_list_lock as _:
            if queue_id in self._id_to_queue:
                return self._id_to_queue[queue_id]

            queue  = JobQueue(queue_id)
            self._id_to_queue[queue_id] = queue
            self.queues.append(queue)
            return queue

    def get_queue_worker_count(self, queue_id: int) -> Optional[int]:
        """
        Retrieve the number of job contexts that have a given `JobQueue` as their wait queue.

        Parameters
        ----------
            queue_id: The application-defined identifier of the queue.

        Returns
        -------
            The number of job management contexts that have the specified queue as their wait queue, or `None` if the given queue identifier is not known.
        """
        with self._queue_list_lock as _:
            if queue_id not in self._id_to_queue:
                return None
            else:
                return _mc.get_worker_count_for_queue(self._internal, queue_id)

    def flush_queues(self) -> None:
        """
        Flush all work items from all `JobQueue` instances registered with the `JobSystem`.
        Any waiting producer threads are woken up.
        """
        with self._queue_list_lock as _:
            for queue in self.queues:
                queue.flush()

    def get_thread(self, thread_id: Optional[int]=None) -> Optional[threading.Thread]:
        """
        Retrieve a reference to the thread given the thread identifier.

        Parameters
        ----------
            thread_id: The lightweight identifier of the thread.

        Returns
        -------
            The associated `threading.Thread` reference. Typically this is derived from `JobSystemThread`, unless `thread_id` specifies the identifier of the main thread.
            Returns `None` if the `thread_id` identifies a thread that is not registered with the `JobSystem` and is not the main application thread.
        """
        if thread_id is None:
            thread_id = threading.get_ident()

        with self._thread_list_lock as _:
            result = self._id_to_thread.get(thread_id, None)
            if result is not None:
                return result

        main_thread = threading.main_thread()
        if main_thread.ident == thread_id:
            return main_thread

        return None
    
    def get_thread_name(self, thread_id: Optional[int]=None) -> str:
        """
        Retrieve the name of a thread given the identifier of the thread.

        Parameters
        ----------
            thread_id: The lightweight identifier of the thread.

        Returns
        -------
            The name of the thread associated with `thread_id`, `Main Thread` if `thread_id` identifies the main application thread, or `Unknown Thread` if `thread_id` is not known to the `JobSystem`.
        """
        if thread_id is None:
            thread_id = threading.get_ident()

        with self._thread_list_lock as _:
            result = self._id_to_thread_name.get(thread_id, None)
            if result is not None:
                return result

        main_thread = threading.main_thread()
        if main_thread.ident == thread_id:
            return 'Main Thread'

        return 'Unknown Thread'

    def register_thread(self, thread: threading.Thread) -> None:
        """
        Register a single thread with the `JobSystem`. The thread should be created, but not started.
        Threads should be registered with the `JobSystem` if they will acquire a `JobContext`.

        Parameters
        ----------
            thread: The thread to register with the `JobSystem`.

        Raises
        ------
            A `ValueError` if `thread` is `None`.
            A `ValueError` if `thread.is_alive()` returns `True` (meaning the thread has already been started).
        """
        if thread is None:
            raise ValueError('The thread argument is None')
        if thread.is_alive():
            raise ValueError('Threads registered with the JobSystem must be in the unstarted state')
        with self._thread_list_lock as _:
            if thread in self.threads:
                return
            else:
                # Note: The thread name is obtained during thread launch.
                self.threads.append(thread)

    def unregister_thread(self, thread: threading.Thread) -> None:
        """
        Unregister a single thread from the `JobSystem`. This would typically happen if the thread crashes.

        Parameters
        ----------
            thread: The thread to unregister.
        """
        if thread is None:
            return

        tid: int = thread.ident
        with self._thread_list_lock as _:
            if tid in self._id_to_thread:
                self._id_to_thread.pop(tid, None)
            if tid in self._id_to_thread_name:
                self._id_to_thread_name.pop(tid, None)
            if thread in self.threads:
                self.threads.remove(thread)

        with self._context_list_lock as _:
            ctx = self._id_to_context.get(tid, None)
            if ctx is not None:
                self._id_to_context.pop(tid, None)
                self.contexts.remove(ctx)

    def launch_threads(self) -> int:
        """
        Launch all threads registered with the `JobSystem`, allowing them to acquire their `JobContext`.

        Returns
        -------
            The number of threads successfully launched.
        """
        thread_list : List[threading.Thread] = None
        name_list   : List[Tuple[int, str]]  = []
        thread_count: int                    = 0

        # Copy the thread list to avoid nested locking.
        with self._thread_list_lock as _:
            thread_list = self.threads.copy()
            self._id_to_thread_name = {}
            self._id_to_thread = {}

        # Start the threads, which assigns an identifier and lets them set their name.
        for thread in thread_list:
            thread.start()
            tid : int = thread.ident
            name: str = thread.name
            if not name:
                name  = 'Unnamed Thread'
            name_list.append((tid, name))

        # All threads have been started, update the thread ID tables.
        with self._thread_list_lock as _:
            for index, item in enumerate(name_list):
                self._id_to_thread     [item[0]] = self.threads[index]
                self._id_to_thread_name[item[0]] = item[1]
                thread_count += 1

        return thread_count

    def terminate_threads(self, timeout: Optional[float]=None) -> None:
        """
        Signal all queues in the `JobSystem` with `JobQueueSignal.TERMINATE` and then wait for all threads to exit.
        If a timeout is specified, the calling thread is blocked for at most `timeout` seconds; otherwise, the calling thread blocks until all threads have exited.

        Parameters
        ----------
            timeout: An optional timeout value, specifying the maximum time, in seconds, to wait for each registered thread to terminate. If this value is `None`, the calling thread may block indefinitely.
        """
        _mc.terminate_job_scheduler(self._internal)
        with self._thread_list_lock as _:
            for thread in self.threads:
                try:
                    if thread.is_alive():
                        thread.join(timeout)
                except Exception as _:
                    pass # Expect that exceptions (TimeoutError, etc.) may be thrown.

    def unregister_all_threads(self) -> List[threading.Thread]:
        """
        Unregister all threads with the `JobSystem`.
        This would typically happen when the thread pool will be re-launched, after calling `JobSystem.terminate_threads`.

        Returns
        -------
            The `list` of registered threads at the time of the call.
        """
        thread_list: List[threading.Thread] = None
        with self._thread_list_lock as _:
            thread_list             = self.threads
            self.threads            = []
            self._id_to_thread      = {}
            self._id_to_thread_name = {}

        with self._context_list_lock as _:
            for thread in thread_list:
                tid: int = thread.ident
                ctx      = self._id_to_context.get(tid, None)
                if ctx is not None:
                    self._id_to_context.pop(tid, None)
                    self.contexts.remove(ctx)

        return thread_list if thread_list is not None else []

    def acquire_context(self, wait_queue: JobQueue, thread_id: Optional[int]=None):
        """
        Acquire a new `JobContext` used to create, submit, execute and/or complete work items.

        Parameters
        ----------
            wait_queue: The `JobQueue` the owning thread will wait on for work and/or submit work to by default.
            thread_id : The identifier of the thread that owns the context, or `None` to retrieve the identifier of the calling thread.

        Returns
        -------
            A new `JobContext` reference bound to the specified thread.
        """
        ctx = JobContext(self, wait_queue, thread_id)
        with self._context_list_lock as _:
            self._id_to_context[ctx.thread_id] = ctx
            self.contexts.append(ctx)
        return ctx


class JobContext:
    """
    Per-thread data used to create, submit, execute, complete and wait for work items.
    Each `JobContext` should only ever be accessed from a single thread.

    Fields
    ------
        queue    : The `JobQueue` on which the owning thread will wait for work, and the default queue to which it will submit work.
        system   : The `JobSystem` from which the context was acquired.
        thread_id: The identifier of the thread that owns the context.
    """
    def __init__(self, owner: JobSystem, wait_queue: JobQueue, thread_id: Optional[int]=None) -> None:
        if owner is None:
            raise ValueError('A valid JobSystem instance must be supplied for the owner argument')
        if wait_queue is None:
            raise ValueError('A valid JobQueue instance must be supplied for the wait_queue argument')
        if thread_id is None:
            thread_id = threading.get_ident()
        if thread_id <= 0:
            raise ValueError(f'An invalid thread_id {thread_id} was specified; valid values are positive integers')

        self.queue    : JobQueue  = wait_queue
        self.system   : JobSystem = owner
        self.thread_id: int       = thread_id
        self._internal: object    = _mc.acquire_job_context(owner._internal, wait_queue._internal, thread_id)

    def __str__(self) -> str:
        return f'JobContext(thread={self.thread_id}, queue={self.queue.id})'

    def __repr__(self) -> str:
        return f'JobContext(thread={self.thread_id}, queue={self.queue.id})'

    def __enter__(self):
        return self

    def __exit__(self, _ex_type, _ex_value, _ex_traceback):
        self.release()

    def release(self) -> None:
        """
        Dispose of resources associated with the `JobContext` and return it to the free pool.
        This function should be called when the owning thread no longer requires use of the `JobContext`.
        """
        if self._internal:
            _mc.release_job_context(self._internal)
            self._internal = None
        self.queue     = None
        self.system    = None
        self.thread_id = None

    def create_job(self, callable: Callable, parent: int=JobId.NONE, *args, **kwargs) -> int:
        """
        Create, but do not submit, a new work item.

        Parameters
        ----------
            callable: The job entry point routine to execute when the job is run. This function will receive arguments (job: int, jobctx: JobContext, *args, **kwargs) and should return an integer result code.
            parent  : The identifier of the job's parent job, or `JobId.NONE` if the new job has no parent.
            args    : Positional arguments to supply to the job entry point `callable` when it runs.
            kwargs  : Keyword arguments to supply to the job entry point `callable` when it runs.

        Returns
        -------
            The identifier of the new job, or `JobId.INVALID` if the job could not be created.
        """
        return _mc.create_python_job(self._internal, self.system._id_to_context, parent, callable, args, kwargs)

    def submit_job(self, job: int, submit: JobSubmitType, target: Optional[JobQueue]=None, dependencies: Optional[List[int]]=None) -> JobSubmitResult:
        """
        Submit a job for execution or cancelation. The job may not be ready to run immediately.

        Parameters
        ----------
            job         : The identifier of the job to submit.
            submit      : One of the values of the `JobSubmitType` enumeration, indicating whether the job should be submitted to run, or immediately canceled.
            target      : The `JobQueue` to which the job should be submitted. This value can be `None`, in which case the job is submitted to the default queue, referenced in `JobContext.queue`.
            dependencies: An optional list of job IDs for jobs that need to complete before this job can run. Specify `None` if the job can run immediately.

        Returns
        -------
            One of the values of the `JobSubmitResult` enumeration, indicating whether the job was successfully submitted (or canceled).
        """
        result: int = _mc.submit_python_job(self._internal, job, target, dependencies, submit)
        return JobSubmitResult(result)

    def cancel_job(self, job: int) -> JobState:
        """
        Attempt to cancel a job that has been previously submitted.
        If the job has already been picked up by a worker and started to run, it is allowed to run to completion.
        Canceling a parent job will also cancel any child jobs that haven't yet started running.

        Parameters
        ----------
            job: The identifier of the job to cancel.

        Returns
        -------
            One of the values of the `JobState` enumeration indicating the state of the job at the time of the call. If the job is successfully canceled, the return value will be `JobState.CANCELED`.
        """
        result: int = _mc.cancel_job(self._internal, job)
        return JobState(result)

    def complete_job(self, job: int) -> None:
        """
        Indicate completion of a job that was previously executed.
        If a job is executed using `JobContext.run_next_job`, there is no need to call this function.
        This function is useful if a job is executed by one context, but completed separately by another context/thread (for example, by notification of completion of an asynchronous operation).

        Parameters
        ----------
            job: The identifier of the completed job.
        """
        _mc.complete_job(self._internal, job)

    def wait_for_job(self, job: int) -> int:
        """
        Perform a busy wait on the calling thread while waiting for a specific job to complete.
        The calling thread runs available jobs while waiting for the specified job to complete.
        The Global Interpreter Lock is released during the wait, allowing other threads to run.
        This function must not be called from the main application thread when execution is performed on a pool of worker threads - instead, use a signaling mechanism like `threading.Event`. Otherwise, a deadlock may result.

        Parameters
        ----------
            job: The identifier of the job to wait for.

        Returns
        -------
            Non-zero if the specified job completed, or zero if the queue became signaled or an error occurred.
        """
        return _mc.wait_for_job(self._internal, job)

    def run_next_job(self) -> int:
        """
        Wait for a ready-to-run job, execute it, and then complete it.
        The calling thread blocks if the `JobQueue` identified by `JobContext.queue` is empty.
        The Global Interpreter Lock is released while waiting for a ready-to-run job, allowing other threads to execute.

        Returns
        -------
            The identifier of the job that was executed, or `JobId.NONE` if the call returned because of a signal on the wait queue.
        """
        return _mc.run_next_job(self._internal)

    def run_next_job_without_completion(self) -> int:
        """
        Wait for a ready-to-run job, and execute it, but do NOT complete it.
        The calling thread blocks if the `JobQueue` identified by `JobContext.queue` is empty.
        The Global Interpreter Lock is released while waiting for a ready-to-run job, allowing other threads to execute.
        Call `JobContext.complete_job` from the `JobContext` owned by the thread that completes the job at some later point.

        Returns
        -------
            The identifier of the job that was executed, or `JobId.NONE` if the call returned because of a signal on the wait queue.
        """
        return _mc.run_next_job_no_completion(self._internal)



class JobSystemThread(threading.Thread):
    """
    The base class for a standard thread of execution that interacts with the `JobSystem`.
    The thread may create and submit jobs, execute jobs, complete jobs, or wait for jobs to complete.

    Fields
    ------
        name      : A `str` specifying a friendly name for the thread, which is used for debugging.
        system    : The `JobSystem` in which the thread participates in work execution.
        context   : The `JobContext` owned by the thread, which is used to interact with the `JobSystem`. This field is only valid after the thread has started.
        wait_queue: The `JobQueue` on which the thread waits for work to execute.
        exit_code : An integer value indicating the reason for thread termination.
    """
    EXIT_SUCCESS  : int =  0 # The thread exited normally
    EXIT_INTERRUPT: int =  1 # The thread exited due to a SIGINT
    EXIT_FAILURE  : int = -1 # Indicates general failure

    def __init__(self, name: str, owner: JobSystem, wait_queue: JobQueue) -> None:
        if owner is None:
            raise ValueError('A valid JobSystem must be supplied for the owner argument')
        if wait_queue is None:
            raise ValueError('A valid JobQueue must be specified for the wait_queue argument')

        super().__init__(name=name)
        self.exit_message: str        = ''
        self.exit_code   : int        = 0
        self.wait_queue  : JobQueue   = wait_queue
        self.system      : JobSystem  = owner
        self.context     : JobContext = None
        owner.register_thread(self)

    def __str__(self) -> str:
        return f'JobSystemThread(id={self.ident}, name={self.name}, queue={self.wait_queue.id})'

    def __repr__(self) -> str:
        return f'JobSystemThread(id={self.ident}, name={self.name}, queue={self.wait_queue.id})'

    def _terminate_thread(self, reason: int=0, message: str='') -> bool:
        """
        Perform thread teardown and cleanup.
        Derived classes may override the default behavior, which is to just save the termination reason and message.

        Parameters
        ----------
            reason : An integer value indicating the reason for thread termination. A value of 0 (`JobSystemThread.EXIT_SUCCESS`) indicates normal, expected shutdown.
            message: A string describing the reason the thread terminated.

        Returns
        -------
            This function always returns `False`.
        """
        if reason == 0 and not message:
            # Provide a default message.
            message = f'The job system thread {self.ident} ({self.name}) terminated normally'

        self.exit_message = message
        self.exit_code    = reason
        return False

    def _handle_signal(self, signal: int) -> bool:
        """
        Handle a signal raised on the wait queue.
        Derived classes may override the default behavior, which is to return `False` if `signal` is `JobQueueSignal.TERMINATE` and ignore the signal and return `True` otherwise.

        Parameters
        ----------
            signal: The wait queue signal value, which has application-defined meaning.

        Returns
        -------
            Return `True` to continue thread execution, or `False` to terminate the thread.
        """
        if signal == JobQueueSignal.TERMINATE:
            return self._terminate_thread()
        else:
            return True

    def _handle_exception(self, exc_info: Exception) -> bool:
        """
        Handle an exception caught by the top-level run loop but otherwise unhandled.
        Derived classes may override the default behavior, which is to terminate the application.
        Note that at this point, the `JobContext` is undefined and should not be referenced.

        Parameters
        ----------
            exc_info: The `Exception` that was caught.

        Returns
        -------
            Return `True` to cleanup and restart, or `False` to terminate the thread.
        """
        return self._terminate_thread(JobSystemThread.EXIT_FAILURE, f'An unhandled exception occurred: {exc_info}')
        

    def _thread_main(self) -> None:
        """
        Implement the main loop of the thread.
        Derived classes may override this method and provide their own behavior if needed.
        """
        ctx    : JobContext = self.context
        queue  : JobQueue   = self.wait_queue
        running: bool       = True
        signal : int        = JobQueueSignal.NONE
        job_id : int        = JobId.NONE
        NO_JOB : int        = JobId.NONE

        if ctx is None:
            return self._terminate_thread(JobSystemThread.EXIT_FAILURE, f'The thread {self.ident} ({self.name}) has no bound JobContext')
        if queue is None:
            return self._terminate_thread(JobSystemThread.EXIT_FAILURE, f'The thread {self.ident} ({self.name}) has no bound JobQueue to wait on')

        while running:
            # Release the GIL and wait for a job to become ready on the queue.
            # If a job is acquired, it is executed and completed and the job exit code is returned.
            job_id = ctx.run_next_job()
            if job_id == NO_JOB:
                signal = queue.get_signal()
                running = self._handle_signal(signal)

    def run(self) -> None:
        """
        The entry point for the thread. Override `JobSystemThread._thread_main` to control the thread run loop.
        """
        running: bool = True
        while running: 
            # Acquire a new, fresh JobContext for executing work.
            with self.system.acquire_context(wait_queue=self.wait_queue, thread_id=self.ident) as ctx:
                self.context = ctx
                try: # Catch exceptions during the main thread loop.
                    self._thread_main()
                    running = False
                except KeyboardInterrupt: # Handle Ctrl+C to shut down.
                    self._terminate_thread(reason=JobSystemThread.EXIT_INTERRUPT, message=f'Job worker thread {self.ident} ({self.name}) terminated; keyboard interrupt received')
                    self.context = None
                    running = False
                except Exception as other: # Handle other exceptions that might occur.
                    running = self._handle_exception(other)
                    self.context = None
