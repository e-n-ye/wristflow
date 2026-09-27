#include <crtdbg.h>
#include <stdlib.h>

/* Run before main so failed assertions cannot wait for a desktop dialog in CI. */
static void __cdecl configure_test_reports(void)
{
    _set_error_mode(_OUT_TO_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

#pragma section(".CRT$XCU", read)
__declspec(allocate(".CRT$XCU")) void (__cdecl *const wristflow_test_runtime_init)(void) = configure_test_reports;
