#include "ram_transition.h"

/* Entire boot ELF is RAM-linked; build verification checks the final location
 * and absence of calls. The update window has no logger, HAL or callbacks. */
__attribute__((noinline)) bool wf_ota_ram_apply(MPI_TypeDef *mpi, const wf_ota_alias_t *next)
{
    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    __DSB(); __ISB();
    /* Close the previous window before changing its offset. Execution,
     * literals, vectors and stack are all in internal RAM. */
    mpi->AASAR = 0; mpi->AAEAR = 0; mpi->AAOAR = 0;
    __DSB(); __ISB();
    mpi->AAOAR = next->offset;
    mpi->AASAR = next->start;
    mpi->AAEAR = next->end;
    __DSB(); __ISB();
    bool matched = mpi->AASAR == next->start && mpi->AAEAR == next->end && mpi->AAOAR == next->offset;
    if (!matched) for (;;) __NOP();
    __set_PRIMASK(saved);
    return matched;
}

/* Retained for static inspection only. Prototype entry never calls this:
 * verified image authentication / durable trial admission is not bound yet.
 * All field offsets are asserted against the assembly below. */
_Static_assert(__builtin_offsetof(wf_ota_boot_plan_t, vector) == 16, "vector offset");
_Static_assert(__builtin_offsetof(wf_ota_boot_plan_t, stack) == 20, "stack offset");
_Static_assert(__builtin_offsetof(wf_ota_boot_plan_t, reset) == 24, "reset offset");
__attribute__((naked, noreturn)) void wf_ota_ram_jump(const wf_ota_boot_plan_t *plan __attribute__((unused)))
{
    __asm volatile(
        "cpsid i\n"
        "ldr r1, [r0, #16]\n"
        "ldr r2, [r0, #20]\n"
        "ldr r3, [r0, #24]\n"
        /* Stop SysTick, clear its pending state/PendSV, disable and clear all
         * implemented external IRQ banks before enabling the target context. */
        "movw r0, #0xe010\n"
        "movt r0, #0xe000\n"
        "movs r4, #0\n"
        "str r4, [r0]\n"
        "str r4, [r0, #8]\n"
        "movw r0, #0xed04\n"
        "movt r0, #0xe000\n"
        "movw r4, #0\n"
        "movt r4, #0x0a00\n"
        "str r4, [r0]\n"
        "movw r0, #0xe004\n"
        "movt r0, #0xe000\n"
        "ldr r4, [r0]\n"
        "and r4, r4, #15\n"
        "adds r4, #1\n"
        "movw r0, #0xe180\n"
        "movt r0, #0xe000\n"
        "mvn r5, #0\n"
        "1:\n"
        "str r5, [r0]\n"
        "str r5, [r0, #256]\n"
        "adds r0, #4\n"
        "subs r4, #1\n"
        "bne 1b\n"
        "movw r0, #0xed08\n"
        "movt r0, #0xe000\n"
        "str r1, [r0]\n" /* VTOR */
        "dsb 0xf\n"
        "isb 0xf\n"
        "movs r0, #0\n"
        "msr control, r0\n"
        "msr basepri, r0\n"
        "msr faultmask, r0\n"
        "msr msplim, r0\n"
        "msr msp, r2\n"
        "isb 0xf\n"
        "cpsie i\n"
        "bx r3\n");
}
