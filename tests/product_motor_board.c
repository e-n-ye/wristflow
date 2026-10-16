#include "../apps/product/src/product_motor_board.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int mode, latch;
static bool fail_mux, wrong_read, open_drain;
static char operations[100];
static unsigned count;
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
int main(void)
{
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
    puts("motor PA20 low preload, mux, latch semantics and failures passed");
    return 0;
}
