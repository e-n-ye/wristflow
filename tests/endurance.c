#include "endurance.h"
#include <assert.h>
int main(void)
{
    wristflow_endurance_t p;
    wristflow_endurance_init(&p, true, true, true);
    assert(!wristflow_endurance_usb(&p, true, true));
    assert(!wristflow_endurance_usb(&p, false, false));
    assert(!wristflow_endurance_usb(&p, true, true));
    assert(!wristflow_endurance_usb(&p, true, false));
    assert(!wristflow_endurance_usb(&p, false, true));
    assert(wristflow_endurance_usb(&p, true, true));
    assert(!wristflow_endurance_hold(&p, true, 1, 2999));
    assert(!wristflow_endurance_hold(&p, false, 1, 2999) && !p.holding);
    assert(!wristflow_endurance_hold(&p, true, 2, 0));
    assert(wristflow_endurance_hold(&p, true, 2, 3000));
    assert(!wristflow_endurance_hold(&p, true, 2, 4000));
    wristflow_endurance_hold(&p, false, 2, 0);
    wristflow_endurance_hold(&p, true, 3, 0);
    wristflow_endurance_block(&p);
    assert(!wristflow_endurance_hold(&p, true, 3, 9000));
    /* The platform may coalesce release and a new press. The new ID cancels
     * the previous press's reminder block without requiring a sampled up. */
    assert(wristflow_endurance_hold(&p, true, 4, 3100));
    return 0;
}
