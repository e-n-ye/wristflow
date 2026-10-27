#ifndef WRISTFLOW_TEST_MEMSIC_FAKE_H
#define WRISTFLOW_TEST_MEMSIC_FAKE_H
extern unsigned memsic_calls, memsic_initials;
extern int memsic_enabled, memsic_fail, memsic_bad, memsic_saturated, memsic_update;
extern void (*memsic_process_hook)(void);
void memsic_reset(void);
#endif
