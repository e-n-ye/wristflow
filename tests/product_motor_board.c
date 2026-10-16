#include "../apps/product/src/product_motor_board.c"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
static int mode, latch;
static bool fail_mux, wrong_read, open_drain;
static char operations[100];
static unsigned count;
static PMUC_TypeDef pmuc;
static unsigned power_reads, output_length;
static char output[1024];
static bool change_register_during_print;
const PMUC_TypeDef *wristflow_test_motor_pmuc(void) { ++power_reads; return &pmuc; }
void rt_kprintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int n = vsnprintf(output + output_length, sizeof output - output_length, format, args);
    va_end(args);
    assert(n > 0 && n < 128 && (unsigned)n < sizeof output - output_length);
    output_length += (unsigned)n;
    if (change_register_during_print) pmuc.PERI_LDO = ~pmuc.PERI_LDO;
}
static void record(char operation) { assert(count + 1 < sizeof operations); operations[count++] = operation; operations[count] = 0; }
void rt_pin_mode(rt_base_t pin, rt_base_t value)
{
    assert(pin == 20);
    if (value == PIN_MODE_INPUT) { record('I'); open_drain = false; }
    else { assert(value == PIN_MODE_OUTPUT && !open_drain && latch == PIN_LOW); record('O'); }
    mode = (int)value;
}
void rt_pin_write(rt_base_t pin, rt_base_t value)
{ assert(pin == 20 && (value == PIN_LOW || value == PIN_HIGH)); latch = (int)value; record(value ? 'H' : 'L'); }
int rt_pin_read(rt_base_t pin)
{ assert(pin == 20 && mode == PIN_MODE_OUTPUT); record('R'); return wrong_read ? !latch : latch; }
int HAL_PIN_Set(int pad, int function, int flags, int hcpu)
{
    assert(pad == PAD_PA20 && function == GPIO_A20 && flags == PIN_PULLDOWN && hcpu == 1);
    assert(mode == PIN_MODE_INPUT && latch == PIN_LOW && !open_drain);
    record('M'); return fail_mux ? -1 : 0;
}
static void reset(void) { count = 0; operations[0] = 0; }
static void check_power(void)
{
    /* All EN/PD combinations and voltage codes, with other rails set, use the
     * pinned SDK register definition rather than a duplicated bit-mask shim. */
    for (unsigned flags = 0; flags < 4; ++flags) {
        for (unsigned code = 0; code < 16; ++code) {
            unsigned flags3 = flags ^ 3, code3 = 15 - code;
            uint32_t raw = 0xa5800021u | (code << 9) | ((flags & 1) << 8) | ((flags >> 1) << 13)
                | (code3 << 17) | ((flags3 & 1) << 16) | ((flags3 >> 1) << 21);
            pmuc.PERI_LDO = raw;
            PMUC_TypeDef before = pmuc;
            reset(); power_reads = output_length = 0; output[0] = 0;
            bool was_configured = configured;
            int old_mode = mode, old_latch = latch;
            assert(wf_motor_power(1, NULL) == RT_EOK);
            assert(power_reads == 1 && !count && configured == was_configured);
            assert(mode == old_mode && latch == old_latch && !memcmp(&pmuc, &before, sizeof pmuc));
            unsigned long printed_raw; unsigned en, pd, printed_code;
            assert(sscanf(output, "[motor-power] PERI_LDO=0x%lx", &printed_raw) == 1);
            assert(printed_raw == raw);
            const char *fields = strstr(output, "[motor-power] LDO2 en_req=");
            assert(fields && sscanf(fields, "[motor-power] LDO2 en_req=%u pd_req=%u vout_code=%u", &en, &pd, &printed_code) == 3);
            assert(en == (flags & 1) && pd == (flags >> 1) && printed_code == code);
            fields = strstr(output, "[motor-power] LDO3 en_req=");
            assert(fields && sscanf(fields, "[motor-power] LDO3 en_req=%u pd_req=%u vout_code=%u", &en, &pd, &printed_code) == 3);
            assert(en == (flags3 & 1) && pd == (flags3 >> 1) && printed_code == code3);
            assert(strstr(output, "requests only; VOUT2 mapping/voltage/current/motion unverified"));
        }
    }
    /* Later output must decode the captured word even if hardware changes. */
    pmuc.PERI_LDO = 0x00001500; power_reads = output_length = 0; output[0] = 0;
    change_register_during_print = true;
    assert(wf_motor_power(1, NULL) == RT_EOK);
    change_register_during_print = false;
    assert(power_reads == 1 && strstr(output, "LDO2 en_req=1 pd_req=0 vout_code=10"));
    assert(strstr(output, "LDO3 en_req=0 pd_req=0 vout_code=0"));
    for (int argc = 0; argc <= 3; ++argc) {
        if (argc == 1) continue;
        reset(); power_reads = output_length = 0; output[0] = 0;
        assert(wf_motor_power(argc, NULL) == -RT_EINVAL && !power_reads && !count);
    }
}
int main(void)
{
    check_power(); /* Also works before GPIO initialization, without changing it. */
    const wristflow_motor_io_t *io = wristflow_product_motor_board_io();
    assert(!io->set_enabled(io->context, true) && !count);
    mode = PIN_MODE_OUTPUT; latch = PIN_HIGH; open_drain = true;
    assert(io->initialize_off(io->context));
    assert(!strcmp(operations, "ILMOR") && latch == PIN_LOW);
    reset(); assert(io->set_enabled(io->context, true)); assert(!strcmp(operations, "HR") && latch == PIN_HIGH);
    reset(); assert(io->set_enabled(io->context, false)); assert(!strcmp(operations, "LR") && latch == PIN_LOW);
    reset(); wrong_read = true;
    assert(!io->set_enabled(io->context, false) && latch == PIN_LOW);
    assert(!io->initialize_off(io->context) && !configured);
    reset(); assert(!io->set_enabled(io->context, true) && !count);
    wrong_read = false; reset(); assert(io->set_enabled(io->context, false));
    assert(!strcmp(operations, "ILMOR"));
    fail_mux = true; reset(); assert(!io->initialize_off(io->context));
    assert(!strcmp(operations, "ILM") && mode == PIN_MODE_INPUT && latch == PIN_LOW);
    reset(); assert(!io->set_enabled(io->context, true) && !count);
    fail_mux = false; reset(); assert(io->set_enabled(io->context, false));
    assert(!strcmp(operations, "ILMOR"));
    check_power();
    puts("motor PA20 low preload, mux, latch semantics and failures passed");
    puts("peripheral LDO diagnostic single read, all EN/PD/codes, no GPIO/register writes passed");
    return 0;
}
