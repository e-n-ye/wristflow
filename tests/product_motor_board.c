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
static unsigned irq;
static bool fail_enable, ignore_enable, ignore_restore, change_voltage, change_other_rail;
const PMUC_TypeDef *wristflow_test_motor_pmuc(void) { ++power_reads; return &pmuc; }
rt_base_t rt_hw_interrupt_disable(void) { return irq++; }
void rt_hw_interrupt_enable(rt_base_t previous) { assert(irq); irq = (unsigned)previous; }
void rt_kprintf(const char *format, ...)
{
    assert(!irq);
    va_list args;
    va_start(args, format);
    int n = vsnprintf(output + output_length, sizeof output - output_length, format, args);
    va_end(args);
    assert(n > 0 && n < 128 && (unsigned)n < sizeof output - output_length);
    output_length += (unsigned)n;
    if (change_register_during_print) pmuc.PERI_LDO = ~pmuc.PERI_LDO;
}
static void record(char operation) { assert(count + 1 < sizeof operations); operations[count++] = operation; operations[count] = 0; }
HAL_StatusTypeDef HAL_PMU_ConfigPeriLdo(PMU_PeriLdoTypeDef ldo, bool en, bool wait)
{
    assert(irq && configured && mode == PIN_MODE_OUTPUT && latch == PIN_LOW);
    assert(ldo == PMU_PERI_LDO3_3V3 && en && !wait && supply_leased && supply_changed);
    record('E');
    if (!ignore_enable)
        pmuc.PERI_LDO = (pmuc.PERI_LDO & ~SUPPLY_CONTROL) | PMUC_PERI_LDO_EN_VDD33_LDO3_Msk;
    return fail_enable ? HAL_ERROR : HAL_OK;
}
void wristflow_test_motor_modify(const volatile uint32_t *reg, uint32_t mask, uint32_t value)
{
    assert(irq && reg == &pmuc.PERI_LDO && latch == PIN_LOW && mask == SUPPLY_CONTROL);
    assert(value == (saved_supply & SUPPLY_CONTROL));
    record('P');
    if (!ignore_restore) pmuc.PERI_LDO = (pmuc.PERI_LDO & ~mask) | value;
}
void HAL_Delay_us(uint32_t us)
{
    assert(!irq && us == 5000 && latch == PIN_LOW && supply_leased && !supply_prepared);
    assert((pmuc.PERI_LDO & SUPPLY_CONTROL) == PMUC_PERI_LDO_EN_VDD33_LDO3_Msk);
    record('D');
    if (change_voltage) pmuc.PERI_LDO ^= 1u << PMUC_PERI_LDO_VDD33_LDO3_SET_VOUT_Pos;
    if (change_other_rail) pmuc.PERI_LDO ^= PMUC_PERI_LDO_EN_VDD33_LDO2_Msk;
}
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
static void supply_fixture(uint32_t raw)
{
    assert(!supply_leased && !irq);
    fail_enable = ignore_enable = ignore_restore = change_voltage = change_other_rail = false;
    configured = true; mode = PIN_MODE_OUTPUT; latch = PIN_LOW; wrong_read = false;
    supply_prepared = false; pmuc.PERI_LDO = raw; reset();
}
static void check_supply(const wristflow_motor_io_t *io)
{
    const uint32_t base = 0x55000719u & ~SUPPLY_CONFIG;
    for (unsigned flags = 0; flags < 4; ++flags) {
        for (unsigned code = 0; code < 16; ++code) {
            uint32_t original = base | (code << 17) | ((flags & 1) << 16) | ((flags >> 1) << 21);
            supply_fixture(original);
            bool ok = io->prepare(io->context);
            if (flags == 3) {
                assert(!ok && !supply_leased && pmuc.PERI_LDO == original && !strcmp(operations, "R"));
                continue;
            }
            assert(ok && latch == PIN_LOW && supply_leased && supply_prepared);
            assert((pmuc.PERI_LDO & SUPPLY_CONTROL) == PMUC_PERI_LDO_EN_VDD33_LDO3_Msk);
            assert((pmuc.PERI_LDO & ~SUPPLY_CONTROL) == (original & ~SUPPLY_CONTROL));
            assert(io->set_enabled(io->context, true));
            assert(io->set_enabled(io->context, false));
            assert(!supply_leased && !supply_prepared && pmuc.PERI_LDO == original);
            assert(!strcmp(operations, flags == 1 ? "RDHRLR" : "REDHRLRP"));
        }
    }
    uint32_t original = base | (4u << 17);
    supply_fixture(original); fail_enable = true;
    assert(!io->prepare(io->context) && supply_leased && !supply_prepared && !strchr(operations, 'H'));
    assert(io->set_enabled(io->context, false) && !supply_leased && pmuc.PERI_LDO == original);
    supply_fixture(original); ignore_enable = true;
    assert(!io->prepare(io->context)); /* HAL_OK without the requested register change. */
    assert(!io->set_enabled(io->context, true) && io->set_enabled(io->context, false));
    assert(!supply_leased && pmuc.PERI_LDO == original);
    supply_fixture(original); change_other_rail = true;
    assert(io->prepare(io->context) && io->set_enabled(io->context, true));
    assert(io->set_enabled(io->context, false));
    assert(pmuc.PERI_LDO == (original ^ PMUC_PERI_LDO_EN_VDD33_LDO2_Msk));
    supply_fixture(original); change_voltage = true;
    assert(!io->prepare(io->context) && !io->set_enabled(io->context, true));
    uint32_t conflicting = pmuc.PERI_LDO;
    assert(!io->set_enabled(io->context, false) && latch == PIN_LOW && supply_leased);
    assert(pmuc.PERI_LDO == conflicting); /* Never overwrite a conflicting owner. */
    pmuc.PERI_LDO ^= 1u << PMUC_PERI_LDO_VDD33_LDO3_SET_VOUT_Pos;
    assert(io->set_enabled(io->context, false) && pmuc.PERI_LDO == original);
    supply_fixture(original); assert(io->prepare(io->context));
    pmuc.PERI_LDO ^= 1u << PMUC_PERI_LDO_VDD33_LDO3_SET_VOUT_Pos;
    assert(!io->set_enabled(io->context, true) && !strchr(operations, 'H'));
    pmuc.PERI_LDO ^= 1u << PMUC_PERI_LDO_VDD33_LDO3_SET_VOUT_Pos;
    assert(io->set_enabled(io->context, false));
    supply_fixture(original); assert(io->prepare(io->context) && io->set_enabled(io->context, true));
    wrong_read = true; assert(!io->set_enabled(io->context, false) && supply_leased && !strchr(operations, 'P'));
    wrong_read = false; ignore_restore = true;
    assert(!io->set_enabled(io->context, false) && supply_leased && latch == PIN_LOW);
    ignore_restore = false;
    assert(io->set_enabled(io->context, false) && !supply_leased && pmuc.PERI_LDO == original);
    supply_fixture(original | PMUC_PERI_LDO_EN_VDD33_LDO3_Msk);
    assert(io->prepare(io->context) && !supply_changed && io->set_enabled(io->context, true));
    pmuc.PERI_LDO ^= 1u << PMUC_PERI_LDO_VDD33_LDO3_SET_VOUT_Pos;
    conflicting = pmuc.PERI_LDO;
    assert(io->set_enabled(io->context, false) && pmuc.PERI_LDO == conflicting && !strchr(operations, 'P'));
}
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
            assert(strstr(output, "LDO3=VOUT2 design; physical voltage/current/motion unmeasured"));
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
    pmuc.PERI_LDO |= PMUC_PERI_LDO_EN_VDD33_LDO3_Msk;
    assert(io->prepare(io->context));
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
    check_supply(io);
    puts("motor PA20 low preload, mux, latch semantics and failures passed");
    puts("peripheral LDO diagnostic single read, all EN/PD/codes, no GPIO/register writes passed");
    puts("LDO3 prepare, exact restore, borrowed supply, conflicts and partial failures passed");
    return 0;
}
