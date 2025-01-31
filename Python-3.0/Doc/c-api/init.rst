.. highlightlang:: c


.. _initialization:

*****************************************
Initialization, Finalization, and Threads
*****************************************


.. cfunction:: void Py_Initialize()

   .. index::
      single: Py_SetProgramName()
      single: PyEval_InitThreads()
      single: PyEval_ReleaseLock()
      single: PyEval_AcquireLock()
      single: modules (in module sys)
      single: path (in module sys)
      module: builtins
      module: __main__
      module: sys
      triple: module; search; path
      single: PySys_SetArgv()
      single: Py_Finalize()

   Initialize the Python interpreter.  In an application embedding  Python, this
   should be called before using any other Python/C API functions; with the
   exception of :cfunc:`Py_SetProgramName`, :cfunc:`PyEval_InitThreads`,
   :cfunc:`PyEval_ReleaseLock`, and :cfunc:`PyEval_AcquireLock`. This initializes
   the table of loaded modules (``sys.modules``), and creates the fundamental
   modules :mod:`builtins`, :mod:`__main__` and :mod:`sys`.  It also initializes
   the module search path (``sys.path``). It does not set ``sys.argv``; use
   :cfunc:`PySys_SetArgv` for that.  This is a no-op when called for a second time
   (without calling :cfunc:`Py_Finalize` first).  There is no return value; it is a
   fatal error if the initialization fails.


.. cfunction:: void Py_InitializeEx(int initsigs)

   This function works like :cfunc:`Py_Initialize` if *initsigs* is 1. If
   *initsigs* is 0, it skips initialization registration of signal handlers, which
   might be useful when Python is embedded.


.. cfunction:: int Py_IsInitialized()

   Return true (nonzero) when the Python interpreter has been initialized, false
   (zero) if not.  After :cfunc:`Py_Finalize` is called, this returns false until
   :cfunc:`Py_Initialize` is called again.


.. cfunction:: void Py_Finalize()

   Undo all initializations made by :cfunc:`Py_Initialize` and subsequent use of
   Python/C API functions, and destroy all sub-interpreters (see
   :cfunc:`Py_NewInterpreter` below) that were created and not yet destroyed since
   the last call to :cfunc:`Py_Initialize`.  Ideally, this frees all memory
   allocated by the Python interpreter.  This is a no-op when called for a second
   time (without calling :cfunc:`Py_Initialize` again first).  There is no return
   value; errors during finalization are ignored.

   This function is provided for a number of reasons.  An embedding application
   might want to restart Python without having to restart the application itself.
   An application that has loaded the Python interpreter from a dynamically
   loadable library (or DLL) might want to free all memory allocated by Python
   before unloading the DLL. During a hunt for memory leaks in an application a
   developer might want to free all memory allocated by Python before exiting from
   the application.

   **Bugs and caveats:** The destruction of modules and objects in modules is done
   in random order; this may cause destructors (:meth:`__del__` methods) to fail
   when they depend on other objects (even functions) or modules.  Dynamically
   loaded extension modules loaded by Python are not unloaded.  Small amounts of
   memory allocated by the Python interpreter may not be freed (if you find a leak,
   please report it).  Memory tied up in circular references between objects is not
   freed.  Some memory allocated by extension modules may not be freed.  Some
   extensions may not work properly if their initialization routine is called more
   than once; this can happen if an application calls :cfunc:`Py_Initialize` and
   :cfunc:`Py_Finalize` more than once.


.. cfunction:: PyThreadState* Py_NewInterpreter()

   .. index::
      module: builtins
      module: __main__
      module: sys
      single: stdout (in module sys)
      single: stderr (in module sys)
      single: stdin (in module sys)

   Create a new sub-interpreter.  This is an (almost) totally separate environment
   for the execution of Python code.  In particular, the new interpreter has
   separate, independent versions of all imported modules, including the
   fundamental modules :mod:`builtins`, :mod:`__main__` and :mod:`sys`.  The
   table of loaded modules (``sys.modules``) and the module search path
   (``sys.path``) are also separate.  The new environment has no ``sys.argv``
   variable.  It has new standard I/O stream file objects ``sys.stdin``,
   ``sys.stdout`` and ``sys.stderr`` (however these refer to the same underlying
   :ctype:`FILE` structures in the C library).

   The return value points to the first thread state created in the new
   sub-interpreter.  This thread state is made in the current thread state.
   Note that no actual thread is created; see the discussion of thread states
   below.  If creation of the new interpreter is unsuccessful, *NULL* is
   returned; no exception is set since the exception state is stored in the
   current thread state and there may not be a current thread state.  (Like all
   other Python/C API functions, the global interpreter lock must be held before
   calling this function and is still held when it returns; however, unlike most
   other Python/C API functions, there needn't be a current thread state on
   entry.)

   .. index::
      single: Py_Finalize()
      single: Py_Initialize()

   Extension modules are shared between (sub-)interpreters as follows: the first
   time a particular extension is imported, it is initialized normally, and a
   (shallow) copy of its module's dictionary is squirreled away.  When the same
   extension is imported by another (sub-)interpreter, a new module is initialized
   and filled with the contents of this copy; the extension's ``init`` function is
   not called.  Note that this is different from what happens when an extension is
   imported after the interpreter has been completely re-initialized by calling
   :cfunc:`Py_Finalize` and :cfunc:`Py_Initialize`; in that case, the extension's
   ``initmodule`` function *is* called again.

   .. index:: single: close() (in module os)

   **Bugs and caveats:** Because sub-interpreters (and the main interpreter) are
   part of the same process, the insulation between them isn't perfect --- for
   example, using low-level file operations like  :func:`os.close` they can
   (accidentally or maliciously) affect each other's open files.  Because of the
   way extensions are shared between (sub-)interpreters, some extensions may not
   work properly; this is especially likely when the extension makes use of
   (static) global variables, or when the extension manipulates its module's
   dictionary after its initialization.  It is possible to insert objects created
   in one sub-interpreter into a namespace of another sub-interpreter; this should
   be done with great care to avoid sharing user-defined functions, methods,
   instances or classes between sub-interpreters, since import operations executed
   by such objects may affect the wrong (sub-)interpreter's dictionary of loaded
   modules.  (XXX This is a hard-to-fix bug that will be addressed in a future
   release.)

   Also note that the use of this functionality is incompatible with extension
   modules such as PyObjC and ctypes that use the :cfunc:`PyGILState_\*` APIs (and
   this is inherent in the way the :cfunc:`PyGILState_\*` functions work).  Simple
   things may work, but confusing behavior will always be near.


.. cfunction:: void Py_EndInterpreter(PyThreadState *tstate)

   .. index:: single: Py_Finalize()

   Destroy the (sub-)interpreter represented by the given thread state. The given
   thread state must be the current thread state.  See the discussion of thread
   states below.  When the call returns, the current thread state is *NULL*.  All
   thread states associated with this interpreter are destroyed.  (The global
   interpreter lock must be held before calling this function and is still held
   when it returns.)  :cfunc:`Py_Finalize` will destroy all sub-interpreters that
   haven't been explicitly destroyed at that point.


.. cfunction:: void Py_SetProgramName(wchar_t *name)

   .. index::
      single: Py_Initialize()
      single: main()
      single: Py_GetPath()

   This function should be called before :cfunc:`Py_Initialize` is called for
   the first time, if it is called at all.  It tells the interpreter the value
   of the ``argv[0]`` argument to the :cfunc:`main` function of the program
   (converted to wide characters).
   This is used by :cfunc:`Py_GetPath` and some other functions below to find
   the Python run-time libraries relative to the interpreter executable.  The
   default value is ``'python'``.  The argument should point to a
   zero-terminated wide character string in static storage whose contents will not
   change for the duration of the program's execution.  No code in the Python
   interpreter will change the contents of this storage.


.. cfunction:: wchar* Py_GetProgramName()

   .. index:: single: Py_SetProgramName()

   Return the program name set with :cfunc:`Py_SetProgramName`, or the default.
   The returned string points into static storage; the caller should not modify its
   value.


.. cfunction:: wchar_t* Py_GetPrefix()

   Return the *prefix* for installed platform-independent files. This is derived
   through a number of complicated rules from the program name set with
   :cfunc:`Py_SetProgramName` and some environment variables; for example, if the
   program name is ``'/usr/local/bin/python'``, the prefix is ``'/usr/local'``. The
   returned string points into static storage; the caller should not modify its
   value.  This corresponds to the :makevar:`prefix` variable in the top-level
   :file:`Makefile` and the :option:`--prefix` argument to the :program:`configure`
   script at build time.  The value is available to Python code as ``sys.prefix``.
   It is only useful on Unix.  See also the next function.


.. cfunction:: wchar_t* Py_GetExecPrefix()

   Return the *exec-prefix* for installed platform-*dependent* files.  This is
   derived through a number of complicated rules from the program name set with
   :cfunc:`Py_SetProgramName` and some environment variables; for example, if the
   program name is ``'/usr/local/bin/python'``, the exec-prefix is
   ``'/usr/local'``.  The returned string points into static storage; the caller
   should not modify its value.  This corresponds to the :makevar:`exec_prefix`
   variable in the top-level :file:`Makefile` and the :option:`--exec-prefix`
   argument to the :program:`configure` script at build  time.  The value is
   available to Python code as ``sys.exec_prefix``.  It is only useful on Unix.

   Background: The exec-prefix differs from the prefix when platform dependent
   files (such as executables and shared libraries) are installed in a different
   directory tree.  In a typical installation, platform dependent files may be
   installed in the :file:`/usr/local/plat` subtree while platform independent may
   be installed in :file:`/usr/local`.

   Generally speaking, a platform is a combination of hardware and software
   families, e.g.  Sparc machines running the Solaris 2.x operating system are
   considered the same platform, but Intel machines running Solaris 2.x are another
   platform, and Intel machines running Linux are yet another platform.  Different
   major revisions of the same operating system generally also form different
   platforms.  Non-Unix operating systems are a different story; the installation
   strategies on those systems are so different that the prefix and exec-prefix are
   meaningless, and set to the empty string. Note that compiled Python bytecode
   files are platform independent (but not independent from the Python version by
   which they were compiled!).

   System administrators will know how to configure the :program:`mount` or
   :program:`automount` programs to share :file:`/usr/local` between platforms
   while having :file:`/usr/local/plat` be a different filesystem for each
   platform.


.. cfunction:: wchar_t* Py_GetProgramFullPath()

   .. index::
      single: Py_SetProgramName()
      single: executable (in module sys)

   Return the full program name of the Python executable; this is  computed as a
   side-effect of deriving the default module search path  from the program name
   (set by :cfunc:`Py_SetProgramName` above). The returned string points into
   static storage; the caller should not modify its value.  The value is available
   to Python code as ``sys.executable``.


.. cfunction:: wchar_t* Py_GetPath()

   .. index::
      triple: module; search; path
      single: path (in module sys)

   Return the default module search path; this is computed from the  program name
   (set by :cfunc:`Py_SetProgramName` above) and some environment variables.  The
   returned string consists of a series of directory names separated by a platform
   dependent delimiter character.  The delimiter character is ``':'`` on Unix and
   Mac OS X, ``';'`` on Windows.  The returned string points into static storage;
   the caller should not modify its value.  The value is available to Python code
   as the list ``sys.path``, which may be modified to change the future search path
   for loaded modules.

   .. XXX should give the exact rules


.. cfunction:: const char* Py_GetVersion()

   Return the version of this Python interpreter.  This is a string that looks
   something like ::

      "3.0a5+ (py3k:63103M, May 12 2008, 00:53:55) \n[GCC 4.2.3]"

   .. index:: single: version (in module sys)

   The first word (up to the first space character) is the current Python version;
   the first three characters are the major and minor version separated by a
   period.  The returned string points into static storage; the caller should not
   modify its value.  The value is available to Python code as :data:`sys.version`.


.. cfunction:: const char* Py_GetBuildNumber()

   Return a string representing the Subversion revision that this Python executable
   was built from.  This number is a string because it may contain a trailing 'M'
   if Python was built from a mixed revision source tree.


.. cfunction:: const char* Py_GetPlatform()

   .. index:: single: platform (in module sys)

   Return the platform identifier for the current platform.  On Unix, this is
   formed from the "official" name of the operating system, converted to lower
   case, followed by the major revision number; e.g., for Solaris 2.x, which is
   also known as SunOS 5.x, the value is ``'sunos5'``.  On Mac OS X, it is
   ``'darwin'``.  On Windows, it is ``'win'``.  The returned string points into
   static storage; the caller should not modify its value.  The value is available
   to Python code as ``sys.platform``.


.. cfunction:: const char* Py_GetCopyright()

   Return the official copyright string for the current Python version, for example

   ``'Copyright 1991-1995 Stichting Mathematisch Centrum, Amsterdam'``

   .. index:: single: copyright (in module sys)

   The returned string points into static storage; the caller should not modify its
   value.  The value is available to Python code as ``sys.copyright``.


.. cfunction:: const char* Py_GetCompiler()

   Return an indication of the compiler used to build the current Python version,
   in square brackets, for example::

      "[GCC 2.7.2.2]"

   .. index:: single: version (in module sys)

   The returned string points into static storage; the caller should not modify its
   value.  The value is available to Python code as part of the variable
   ``sys.version``.


.. cfunction:: const char* Py_GetBuildInfo()

   Return information about the sequence number and build date and time  of the
   current Python interpreter instance, for example ::

      "#67, Aug  1 1997, 22:34:28"

   .. index:: single: version (in module sys)

   The returned string points into static storage; the caller should not modify its
   value.  The value is available to Python code as part of the variable
   ``sys.version``.


.. cfunction:: void PySys_SetArgv(int argc, wchar_t **argv)

   .. index::
      single: main()
      single: Py_FatalError()
      single: argv (in module sys)

   Set ``sys.argv`` based on *argc* and *argv*.  These parameters are similar to
   those passed to the program's :cfunc:`main` function with the difference that
   the first entry should refer to the script file to be executed rather than the
   executable hosting the Python interpreter.  If there isn't a script that will be
   run, the first entry in *argv* can be an empty string.  If this function fails
   to initialize ``sys.argv``, a fatal condition is signalled using
   :cfunc:`Py_FatalError`.

   .. XXX impl. doesn't seem consistent in allowing 0/NULL for the params;
      check w/ Guido.


.. _threads:

Thread State and the Global Interpreter Lock
============================================

.. index::
   single: global interpreter lock
   single: interpreter lock
   single: lock, interpreter

The Python interpreter is not fully thread safe.  In order to support
multi-threaded Python programs, there's a global lock that must be held by the
current thread before it can safely access Python objects. Without the lock,
even the simplest operations could cause problems in a multi-threaded program:
for example, when two threads simultaneously increment the reference count of
the same object, the reference count could end up being incremented only once
instead of twice.

.. index:: single: setcheckinterval() (in module sys)

Therefore, the rule exists that only the thread that has acquired the global
interpreter lock may operate on Python objects or call Python/C API functions.
In order to support multi-threaded Python programs, the interpreter regularly
releases and reacquires the lock --- by default, every 100 bytecode instructions
(this can be changed with  :func:`sys.setcheckinterval`).  The lock is also
released and reacquired around potentially blocking I/O operations like reading
or writing a file, so that other threads can run while the thread that requests
the I/O is waiting for the I/O operation to complete.

.. index::
   single: PyThreadState
   single: PyThreadState

The Python interpreter needs to keep some bookkeeping information separate per
thread --- for this it uses a data structure called :ctype:`PyThreadState`.
There's one global variable, however: the pointer to the current
:ctype:`PyThreadState` structure.  While most thread packages have a way to
store "per-thread global data," Python's internal platform independent thread
abstraction doesn't support this yet.  Therefore, the current thread state must
be manipulated explicitly.

This is easy enough in most cases.  Most code manipulating the global
interpreter lock has the following simple structure::

   Save the thread state in a local variable.
   Release the interpreter lock.
   ...Do some blocking I/O operation...
   Reacquire the interpreter lock.
   Restore the thread state from the local variable.

This is so common that a pair of macros exists to simplify it::

   Py_BEGIN_ALLOW_THREADS
   ...Do some blocking I/O operation...
   Py_END_ALLOW_THREADS

.. index::
   single: Py_BEGIN_ALLOW_THREADS
   single: Py_END_ALLOW_THREADS

The :cmacro:`Py_BEGIN_ALLOW_THREADS` macro opens a new block and declares a
hidden local variable; the :cmacro:`Py_END_ALLOW_THREADS` macro closes the
block.  Another advantage of using these two macros is that when Python is
compiled without thread support, they are defined empty, thus saving the thread
state and lock manipulations.

When thread support is enabled, the block above expands to the following code::

   PyThreadState *_save;

   _save = PyEval_SaveThread();
   ...Do some blocking I/O operation...
   PyEval_RestoreThread(_save);

Using even lower level primitives, we can get roughly the same effect as
follows::

   PyThreadState *_save;

   _save = PyThreadState_Swap(NULL);
   PyEval_ReleaseLock();
   ...Do some blocking I/O operation...
   PyEval_AcquireLock();
   PyThreadState_Swap(_save);

.. index::
   single: PyEval_RestoreThread()
   single: errno
   single: PyEval_SaveThread()
   single: PyEval_ReleaseLock()
   single: PyEval_AcquireLock()

There are some subtle differences; in particular, :cfunc:`PyEval_RestoreThread`
saves and restores the value of the  global variable :cdata:`errno`, since the
lock manipulation does not guarantee that :cdata:`errno` is left alone.  Also,
when thread support is disabled, :cfunc:`PyEval_SaveThread` and
:cfunc:`PyEval_RestoreThread` don't manipulate the lock; in this case,
:cfunc:`PyEval_ReleaseLock` and :cfunc:`PyEval_AcquireLock` are not available.
This is done so that dynamically loaded extensions compiled with thread support
enabled can be loaded by an interpreter that was compiled with disabled thread
support.

The global interpreter lock is used to protect the pointer to the current thread
state.  When releasing the lock and saving the thread state, the current thread
state pointer must be retrieved before the lock is released (since another
thread could immediately acquire the lock and store its own thread state in the
global variable). Conversely, when acquiring the lock and restoring the thread
state, the lock must be acquired before storing the thread state pointer.

Why am I going on with so much detail about this?  Because when threads are
created from C, they don't have the global interpreter lock, nor is there a
thread state data structure for them.  Such threads must bootstrap themselves
into existence, by first creating a thread state data structure, then acquiring
the lock, and finally storing their thread state pointer, before they can start
using the Python/C API.  When they are done, they should reset the thread state
pointer, release the lock, and finally free their thread state data structure.

Threads can take advantage of the :cfunc:`PyGILState_\*` functions to do all of
the above automatically.  The typical idiom for calling into Python from a C
thread is now::

   PyGILState_STATE gstate;
   gstate = PyGILState_Ensure();

   /* Perform Python actions here.  */
   result = CallSomeFunction();
   /* evaluate result */

   /* Release the thread. No Python API allowed beyond this point. */
   PyGILState_Release(gstate);

Note that the :cfunc:`PyGILState_\*` functions assume there is only one global
interpreter (created automatically by :cfunc:`Py_Initialize`).  Python still
supports the creation of additional interpreters (using
:cfunc:`Py_NewInterpreter`), but mixing multiple interpreters and the
:cfunc:`PyGILState_\*` API is unsupported.


.. ctype:: PyInterpreterState

   This data structure represents the state shared by a number of cooperating
   threads.  Threads belonging to the same interpreter share their module
   administration and a few other internal items. There are no public members in
   this structure.

   Threads belonging to different interpreters initially share nothing, except
   process state like available memory, open file descriptors and such.  The global
   interpreter lock is also shared by all threads, regardless of to which
   interpreter they belong.


.. ctype:: PyThreadState

   This data structure represents the state of a single thread.  The only public
   data member is :ctype:`PyInterpreterState \*`:attr:`interp`, which points to
   this thread's interpreter state.


.. cfunction:: void PyEval_InitThreads()

   .. index::
      single: PyEval_ReleaseLock()
      single: PyEval_ReleaseThread()
      single: PyEval_SaveThread()
      single: PyEval_RestoreThread()

   Initialize and acquire the global interpreter lock.  It should be called in the
   main thread before creating a second thread or engaging in any other thread
   operations such as :cfunc:`PyEval_ReleaseLock` or
   ``PyEval_ReleaseThread(tstate)``. It is not needed before calling
   :cfunc:`PyEval_SaveThread` or :cfunc:`PyEval_RestoreThread`.

   .. index:: single: Py_Initialize()

   This is a no-op when called for a second time.  It is safe to call this function
   before calling :cfunc:`Py_Initialize`.

   .. index:: module: _thread

   When only the main thread exists, no lock operations are needed. This is a
   common situation (most Python programs do not use threads), and the lock
   operations slow the interpreter down a bit. Therefore, the lock is not created
   initially.  This situation is equivalent to having acquired the lock:  when
   there is only a single thread, all object accesses are safe.  Therefore, when
   this function initializes the lock, it also acquires it.  Before the Python
   :mod:`_thread` module creates a new thread, knowing that either it has the lock
   or the lock hasn't been created yet, it calls :cfunc:`PyEval_InitThreads`.  When
   this call returns, it is guaranteed that the lock has been created and that the
   calling thread has acquired it.

   It is **not** safe to call this function when it is unknown which thread (if
   any) currently has the global interpreter lock.

   This function is not available when thread support is disabled at compile time.


.. cfunction:: int PyEval_ThreadsInitialized()

   Returns a non-zero value if :cfunc:`PyEval_InitThreads` has been called.  This
   function can be called without holding the lock, and therefore can be used to
   avoid calls to the locking API when running single-threaded.  This function is
   not available when thread support is disabled at compile time.


.. cfunction:: void PyEval_AcquireLock()

   Acquire the global interpreter lock.  The lock must have been created earlier.
   If this thread already has the lock, a deadlock ensues.  This function is not
   available when thread support is disabled at compile time.


.. cfunction:: void PyEval_ReleaseLock()

   Release the global interpreter lock.  The lock must have been created earlier.
   This function is not available when thread support is disabled at compile time.


.. cfunction:: void PyEval_AcquireThread(PyThreadState *tstate)

   Acquire the global interpreter lock and set the current thread state to
   *tstate*, which should not be *NULL*.  The lock must have been created earlier.
   If this thread already has the lock, deadlock ensues.  This function is not
   available when thread support is disabled at compile time.


.. cfunction:: void PyEval_ReleaseThread(PyThreadState *tstate)

   Reset the current thread state to *NULL* and release the global interpreter
   lock.  The lock must have been created earlier and must be held by the current
   thread.  The *tstate* argument, which must not be *NULL*, is only used to check
   that it represents the current thread state --- if it isn't, a fatal error is
   reported. This function is not available when thread support is disabled at
   compile time.


.. cfunction:: PyThreadState* PyEval_SaveThread()

   Release the interpreter lock (if it has been created and thread support is
   enabled) and reset the thread state to *NULL*, returning the previous thread
   state (which is not *NULL*).  If the lock has been created, the current thread
   must have acquired it.  (This function is available even when thread support is
   disabled at compile time.)


.. cfunction:: void PyEval_RestoreThread(PyThreadState *tstate)

   Acquire the interpreter lock (if it has been created and thread support is
   enabled) and set the thread state to *tstate*, which must not be *NULL*.  If the
   lock has been created, the current thread must not have acquired it, otherwise
   deadlock ensues.  (This function is available even when thread support is
   disabled at compile time.)


.. cfunction:: void PyEval_ReInitThreads()

   This function is called from :cfunc:`PyOS_AfterFork` to ensure that newly
   created child processes don't hold locks referring to threads which
   are not running in the child process.


The following macros are normally used without a trailing semicolon; look for
example usage in the Python source distribution.


.. cmacro:: Py_BEGIN_ALLOW_THREADS

   This macro expands to ``{ PyThreadState *_save; _save = PyEval_SaveThread();``.
   Note that it contains an opening brace; it must be matched with a following
   :cmacro:`Py_END_ALLOW_THREADS` macro.  See above for further discussion of this
   macro.  It is a no-op when thread support is disabled at compile time.


.. cmacro:: Py_END_ALLOW_THREADS

   This macro expands to ``PyEval_RestoreThread(_save); }``. Note that it contains
   a closing brace; it must be matched with an earlier
   :cmacro:`Py_BEGIN_ALLOW_THREADS` macro.  See above for further discussion of
   this macro.  It is a no-op when thread support is disabled at compile time.


.. cmacro:: Py_BLOCK_THREADS

   This macro expands to ``PyEval_RestoreThread(_save);``: it is equivalent to
   :cmacro:`Py_END_ALLOW_THREADS` without the closing brace.  It is a no-op when
   thread support is disabled at compile time.


.. cmacro:: Py_UNBLOCK_THREADS

   This macro expands to ``_save = PyEval_SaveThread();``: it is equivalent to
   :cmacro:`Py_BEGIN_ALLOW_THREADS` without the opening brace and variable
   declaration.  It is a no-op when thread support is disabled at compile time.

All of the following functions are only available when thread support is enabled
at compile time, and must be called only when the interpreter lock has been
created.


.. cfunction:: PyInterpreterState* PyInterpreterState_New()

   Create a new interpreter state object.  The interpreter lock need not be held,
   but may be held if it is necessary to serialize calls to this function.


.. cfunction:: void PyInterpreterState_Clear(PyInterpreterState *interp)

   Reset all information in an interpreter state object.  The interpreter lock must
   be held.


.. cfunction:: void PyInterpreterState_Delete(PyInterpreterState *interp)

   Destroy an interpreter state object.  The interpreter lock need not be held.
   The interpreter state must have been reset with a previous call to
   :cfunc:`PyInterpreterState_Clear`.


.. cfunction:: PyThreadState* PyThreadState_New(PyInterpreterState *interp)

   Create a new thread state object belonging to the given interpreter object.  The
   interpreter lock need not be held, but may be held if it is necessary to
   serialize calls to this function.


.. cfunction:: void PyThreadState_Clear(PyThreadState *tstate)

   Reset all information in a thread state object.  The interpreter lock must be
   held.


.. cfunction:: void PyThreadState_Delete(PyThreadState *tstate)

   Destroy a thread state object.  The interpreter lock need not be held.  The
   thread state must have been reset with a previous call to
   :cfunc:`PyThreadState_Clear`.


.. cfunction:: PyThreadState* PyThreadState_Get()

   Return the current thread state.  The interpreter lock must be held.  When the
   current thread state is *NULL*, this issues a fatal error (so that the caller
   needn't check for *NULL*).


.. cfunction:: PyThreadState* PyThreadState_Swap(PyThreadState *tstate)

   Swap the current thread state with the thread state given by the argument
   *tstate*, which may be *NULL*.  The interpreter lock must be held.


.. cfunction:: PyObject* PyThreadState_GetDict()

   Return a dictionary in which extensions can store thread-specific state
   information.  Each extension should use a unique key to use to store state in
   the dictionary.  It is okay to call this function when no current thread state
   is available. If this function returns *NULL*, no exception has been raised and
   the caller should assume no current thread state is available.


.. cfunction:: int PyThreadState_SetAsyncExc(long id, PyObject *exc)

   Asynchronously raise an exception in a thread. The *id* argument is the thread
   id of the target thread; *exc* is the exception object to be raised. This
   function does not steal any references to *exc*. To prevent naive misuse, you
   must write your own C extension to call this.  Must be called with the GIL held.
   Returns the number of thread states modified; this is normally one, but will be
   zero if the thread id isn't found.  If *exc* is :const:`NULL`, the pending
   exception (if any) for the thread is cleared. This raises no exceptions.


.. cfunction:: PyGILState_STATE PyGILState_Ensure()

   Ensure that the current thread is ready to call the Python C API regardless of
   the current state of Python, or of its thread lock. This may be called as many
   times as desired by a thread as long as each call is matched with a call to
   :cfunc:`PyGILState_Release`. In general, other thread-related APIs may be used
   between :cfunc:`PyGILState_Ensure` and :cfunc:`PyGILState_Release` calls as long
   as the thread state is restored to its previous state before the Release().  For
   example, normal usage of the :cmacro:`Py_BEGIN_ALLOW_THREADS` and
   :cmacro:`Py_END_ALLOW_THREADS` macros is acceptable.

   The return value is an opaque "handle" to the thread state when
   :cfunc:`PyGILState_Ensure` was called, and must be passed to
   :cfunc:`PyGILState_Release` to ensure Python is left in the same state. Even
   though recursive calls are allowed, these handles *cannot* be shared - each
   unique call to :cfunc:`PyGILState_Ensure` must save the handle for its call
   to :cfunc:`PyGILState_Release`.

   When the function returns, the current thread will hold the GIL. Failure is a
   fatal error.


.. cfunction:: void PyGILState_Release(PyGILState_STATE)

   Release any resources previously acquired.  After this call, Python's state will
   be the same as it was prior to the corresponding :cfunc:`PyGILState_Ensure` call
   (but generally this state will be unknown to the caller, hence the use of the
   GILState API.)

   Every call to :cfunc:`PyGILState_Ensure` must be matched by a call to
   :cfunc:`PyGILState_Release` on the same thread.


.. _profiling:

Profiling and Tracing
=====================

.. sectionauthor:: Fred L. Drake, Jr. <fdrake@acm.org>


The Python interpreter provides some low-level support for attaching profiling
and execution tracing facilities.  These are used for profiling, debugging, and
coverage analysis tools.

This C interface allows the profiling or tracing code to avoid the overhead of
calling through Python-level callable objects, making a direct C function call
instead.  The essential attributes of the facility have not changed; the
interface allows trace functions to be installed per-thread, and the basic
events reported to the trace function are the same as had been reported to the
Python-level trace functions in previous versions.


.. ctype:: int (*Py_tracefunc)(PyObject *obj, PyFrameObject *frame, int what, PyObject *arg)

   The type of the trace function registered using :cfunc:`PyEval_SetProfile` and
   :cfunc:`PyEval_SetTrace`. The first parameter is the object passed to the
   registration function as *obj*, *frame* is the frame object to which the event
   pertains, *what* is one of the constants :const:`PyTrace_CALL`,
   :const:`PyTrace_EXCEPTION`, :const:`PyTrace_LINE`, :const:`PyTrace_RETURN`,
   :const:`PyTrace_C_CALL`, :const:`PyTrace_C_EXCEPTION`, or
   :const:`PyTrace_C_RETURN`, and *arg* depends on the value of *what*:

   +------------------------------+--------------------------------------+
   | Value of *what*              | Meaning of *arg*                     |
   +==============================+======================================+
   | :const:`PyTrace_CALL`        | Always *NULL*.                       |
   +------------------------------+--------------------------------------+
   | :const:`PyTrace_EXCEPTION`   | Exception information as returned by |
   |                              | :func:`sys.exc_info`.                |
   +------------------------------+--------------------------------------+
   | :const:`PyTrace_LINE`        | Always *NULL*.                       |
   +------------------------------+--------------------------------------+
   | :const:`PyTrace_RETURN`      | Value being returned to the caller.  |
   +------------------------------+--------------------------------------+
   | :const:`PyTrace_C_CALL`      | Name of function being called.       |
   +------------------------------+--------------------------------------+
   | :const:`PyTrace_C_EXCEPTION` | Always *NULL*.                       |
   +------------------------------+--------------------------------------+
   | :const:`PyTrace_C_RETURN`    | Always *NULL*.                       |
   +------------------------------+--------------------------------------+


.. cvar:: int PyTrace_CALL

   The value of the *what* parameter to a :ctype:`Py_tracefunc` function when a new
   call to a function or method is being reported, or a new entry into a generator.
   Note that the creation of the iterator for a generator function is not reported
   as there is no control transfer to the Python bytecode in the corresponding
   frame.


.. cvar:: int PyTrace_EXCEPTION

   The value of the *what* parameter to a :ctype:`Py_tracefunc` function when an
   exception has been raised.  The callback function is called with this value for
   *what* when after any bytecode is processed after which the exception becomes
   set within the frame being executed.  The effect of this is that as exception
   propagation causes the Python stack to unwind, the callback is called upon
   return to each frame as the exception propagates.  Only trace functions receives
   these events; they are not needed by the profiler.


.. cvar:: int PyTrace_LINE

   The value passed as the *what* parameter to a trace function (but not a
   profiling function) when a line-number event is being reported.


.. cvar:: int PyTrace_RETURN

   The value for the *what* parameter to :ctype:`Py_tracefunc` functions when a
   call is returning without propagating an exception.


.. cvar:: int PyTrace_C_CALL

   The value for the *what* parameter to :ctype:`Py_tracefunc` functions when a C
   function is about to be called.


.. cvar:: int PyTrace_C_EXCEPTION

   The value for the *what* parameter to :ctype:`Py_tracefunc` functions when a C
   function has thrown an exception.


.. cvar:: int PyTrace_C_RETURN

   The value for the *what* parameter to :ctype:`Py_tracefunc` functions when a C
   function has returned.


.. cfunction:: void PyEval_SetProfile(Py_tracefunc func, PyObject *obj)

   Set the profiler function to *func*.  The *obj* parameter is passed to the
   function as its first parameter, and may be any Python object, or *NULL*.  If
   the profile function needs to maintain state, using a different value for *obj*
   for each thread provides a convenient and thread-safe place to store it.  The
   profile function is called for all monitored events except the line-number
   events.


.. cfunction:: void PyEval_SetTrace(Py_tracefunc func, PyObject *obj)

   Set the tracing function to *func*.  This is similar to
   :cfunc:`PyEval_SetProfile`, except the tracing function does receive line-number
   events.

.. cfunction:: PyObject* PyEval_GetCallStats(PyObject *self)

   Return a tuple of function call counts.  There are constants defined for the
   positions within the tuple:
   
   +-------------------------------+-------+
   | Name                          | Value |
   +===============================+=======+
   | :const:`PCALL_ALL`            | 0     |
   +-------------------------------+-------+
   | :const:`PCALL_FUNCTION`       | 1     |
   +-------------------------------+-------+
   | :const:`PCALL_FAST_FUNCTION`  | 2     |
   +-------------------------------+-------+
   | :const:`PCALL_FASTER_FUNCTION`| 3     |
   +-------------------------------+-------+
   | :const:`PCALL_METHOD`         | 4     |
   +-------------------------------+-------+
   | :const:`PCALL_BOUND_METHOD`   | 5     |
   +-------------------------------+-------+
   | :const:`PCALL_CFUNCTION`      | 6     |
   +-------------------------------+-------+
   | :const:`PCALL_TYPE`           | 7     |
   +-------------------------------+-------+
   | :const:`PCALL_GENERATOR`      | 8     |
   +-------------------------------+-------+
   | :const:`PCALL_OTHER`          | 9     |
   +-------------------------------+-------+
   | :const:`PCALL_POP`            | 10    |
   +-------------------------------+-------+
   
   :const:`PCALL_FAST_FUNCTION` means no argument tuple needs to be created.
   :const:`PCALL_FASTER_FUNCTION` means that the fast-path frame setup code is used.

   If there is a method call where the call can be optimized by changing
   the argument tuple and calling the function directly, it gets recorded
   twice.

   This function is only present if Python is compiled with :const:`CALL_PROFILE`
   defined.

.. _advanced-debugging:

Advanced Debugger Support
=========================

.. sectionauthor:: Fred L. Drake, Jr. <fdrake@acm.org>


These functions are only intended to be used by advanced debugging tools.


.. cfunction:: PyInterpreterState* PyInterpreterState_Head()

   Return the interpreter state object at the head of the list of all such objects.


.. cfunction:: PyInterpreterState* PyInterpreterState_Next(PyInterpreterState *interp)

   Return the next interpreter state object after *interp* from the list of all
   such objects.


.. cfunction:: PyThreadState * PyInterpreterState_ThreadHead(PyInterpreterState *interp)

   Return the a pointer to the first :ctype:`PyThreadState` object in the list of
   threads associated with the interpreter *interp*.


.. cfunction:: PyThreadState* PyThreadState_Next(PyThreadState *tstate)

   Return the next thread state object after *tstate* from the list of all such
   objects belonging to the same :ctype:`PyInterpreterState` object.

