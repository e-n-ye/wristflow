"""Owned sftool process tree. Windows creates the child inside a kill job.

No shell or device commands live here; the OTA adapter supplies a fixed argv.
"""
import os
from pathlib import Path
import signal
import subprocess


class ReaderCleanupError(OSError):
    """Keep the owner reachable when a failed constructor cannot confirm cleanup."""
    def __init__(self, owner, detail):
        super().__init__(detail)
        self.owner = owner


class ReadProcess:
    def __init__(self, argv, directory):
        self.child = self.job = self.handle = None
        self.log = (Path(directory) / "sftool.log").open("xb")
        try:
            if os.name == "nt":
                self._windows(argv, directory)
            else:
                self.child = subprocess.Popen(argv, cwd=directory, stdin=subprocess.DEVNULL,
                    stdout=self.log, stderr=subprocess.STDOUT, start_new_session=True)
        except BaseException:
            try:
                self.close()
            except BaseException as cleanup:
                raise ReaderCleanupError(self, "Reader launch cleanup uncertain: " + repr(cleanup)) from cleanup
            raise

    def _windows(self, argv, directory):
        import ctypes as c
        from ctypes import wintypes as w
        import msvcrt
        self.api = c.WinDLL("kernel32", use_last_error=True)

        class Startup(c.Structure):
            _fields_ = [("cb", w.DWORD), ("reserved", w.LPWSTR), ("desktop", w.LPWSTR),
                ("title", w.LPWSTR), ("x", w.DWORD), ("y", w.DWORD),
                ("xsize", w.DWORD), ("ysize", w.DWORD), ("xchars", w.DWORD),
                ("ychars", w.DWORD), ("fill", w.DWORD), ("flags", w.DWORD),
                ("show", w.WORD), ("reserved_size", w.WORD), ("reserved2", c.POINTER(c.c_byte)),
                ("stdin", w.HANDLE), ("stdout", w.HANDLE), ("stderr", w.HANDLE)]
        class StartupEx(c.Structure):
            _fields_ = [("startup", Startup), ("attributes", c.c_void_p)]
        class Process(c.Structure):
            _fields_ = [("process", w.HANDLE), ("thread", w.HANDLE), ("pid", w.DWORD), ("tid", w.DWORD)]
        class BasicLimit(c.Structure):
            _fields_ = [("process_time", c.c_int64), ("job_time", c.c_int64),
                ("flags", w.DWORD), ("min_working", c.c_size_t), ("max_working", c.c_size_t),
                ("active", w.DWORD), ("affinity", c.c_size_t), ("priority", w.DWORD), ("scheduling", w.DWORD)]
        class ExtendedLimit(c.Structure):
            _fields_ = [("basic", BasicLimit), ("io", c.c_uint64 * 6),
                ("process_memory", c.c_size_t), ("job_memory", c.c_size_t),
                ("peak_process", c.c_size_t), ("peak_job", c.c_size_t)]
        signatures = {
            "CreateJobObjectW": ([c.c_void_p, w.LPCWSTR], w.HANDLE),
            "SetInformationJobObject": ([w.HANDLE, c.c_int, c.c_void_p, w.DWORD], w.BOOL),
            "CreateProcessW": ([w.LPCWSTR, w.LPWSTR, c.c_void_p, c.c_void_p, w.BOOL,
                w.DWORD, c.c_void_p, w.LPCWSTR, c.c_void_p, c.POINTER(Process)], w.BOOL),
            "InitializeProcThreadAttributeList": ([c.c_void_p, w.DWORD, w.DWORD, c.POINTER(c.c_size_t)], w.BOOL),
            "UpdateProcThreadAttribute": ([c.c_void_p, w.DWORD, c.c_size_t, c.c_void_p,
                c.c_size_t, c.c_void_p, c.c_void_p], w.BOOL),
            "DeleteProcThreadAttributeList": ([c.c_void_p], None),
            "ResumeThread": ([w.HANDLE], w.DWORD),
            "TerminateProcess": ([w.HANDLE, w.UINT], w.BOOL),
            "TerminateJobObject": ([w.HANDLE, w.UINT], w.BOOL),
            "WaitForSingleObject": ([w.HANDLE, w.DWORD], w.DWORD),
            "GetExitCodeProcess": ([w.HANDLE, c.POINTER(w.DWORD)], w.BOOL),
            "CloseHandle": ([w.HANDLE], w.BOOL),
        }
        for name, (args, result) in signatures.items():
            method = getattr(self.api, name)
            method.argtypes, method.restype = args, result
        def require(ok):
            if not ok:
                raise c.WinError(c.get_last_error())
        self.job = self.api.CreateJobObjectW(None, None)
        require(self.job)
        limit = ExtendedLimit()
        limit.basic.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        require(self.api.SetInformationJobObject(self.job, 9, c.byref(limit), c.sizeof(limit)))
        info = Process()
        with open(os.devnull, "rb") as null:
            handles = [msvcrt.get_osfhandle(f.fileno()) for f in (null, self.log)]
            size = c.c_size_t()
            self.api.InitializeProcThreadAttributeList(None, 2, 0, c.byref(size))
            attributes = c.create_string_buffer(size.value)
            require(self.api.InitializeProcThreadAttributeList(attributes, 2, 0, c.byref(size)))
            try:
                for handle in handles:
                    os.set_handle_inheritable(handle, True)
                inherited = (w.HANDLE * len(handles))(*handles)
                jobs = (w.HANDLE * 1)(self.job)
                require(self.api.UpdateProcThreadAttribute(attributes, 0, 0x20002,
                    inherited, c.sizeof(inherited), None, None))  # HANDLE_LIST
                require(self.api.UpdateProcThreadAttribute(attributes, 0, 0x2000d,
                    jobs, c.sizeof(jobs), None, None))  # JOB_LIST (Windows 10+)
                startup = StartupEx(startup=Startup(cb=c.sizeof(StartupEx), flags=0x100,
                    stdin=handles[0], stdout=handles[1], stderr=handles[1]),
                    attributes=c.cast(attributes, c.c_void_p))
                command = c.create_unicode_buffer(subprocess.list2cmdline([str(x) for x in argv]))
                require(self.api.CreateProcessW(str(argv[0]), command, None, None, True,
                    0x08080004, None, str(directory), c.byref(startup), c.byref(info)))
                self.handle = info.process  # Record ownership before any fallible cleanup.
                try:
                    if self.api.ResumeThread(info.thread) == 0xffffffff:
                        raise c.WinError(c.get_last_error())
                finally:
                    self.api.CloseHandle(info.thread)
            finally:
                try:
                    for handle in handles:
                        os.set_handle_inheritable(handle, False)
                finally:
                    self.api.DeleteProcThreadAttributeList(attributes)

    def poll(self):
        if self.child is not None:
            return self.child.poll()
        import ctypes as c
        from ctypes import wintypes as w
        state = self.api.WaitForSingleObject(self.handle, 0)
        if state == 258:
            return None
        if state != 0:
            raise c.WinError(c.get_last_error())
        code = w.DWORD()
        if not self.api.GetExitCodeProcess(self.handle, c.byref(code)):
            raise c.WinError(c.get_last_error())
        return code.value

    def close(self):
        """Kill owned descendants even if the leader already exited; then reap it."""
        try:
            if self.child is not None:
                try:
                    os.killpg(self.child.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                self.child.wait(timeout=5)
            if self.job:
                self.api.TerminateJobObject(self.job, 1)
                # Closing the last job handle also kills descendants on parent exit.
                if not self.api.CloseHandle(self.job):
                    raise OSError("Cannot close owned reader job")
                self.job = None
            if self.handle:
                if self.api.WaitForSingleObject(self.handle, 5000) != 0:
                    raise OSError("Owned reader exit not confirmed")
                if not self.api.CloseHandle(self.handle):
                    raise OSError("Cannot close owned reader handle")
                self.handle = None
        finally:
            self.log.close()
