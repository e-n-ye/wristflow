#ifdef NDEBUG
#undef NDEBUG
#endif
#include "auto_brightness.h"
#include <assert.h>
int main(void)
{
    uint32_t mlux = 123;
    assert(wf_light_millilux(0,0,&mlux) && mlux==0);
    assert(wf_light_millilux(1000,0,&mlux) && mlux==1774300);
    assert(wf_light_millilux(550,450,&mlux) && mlux==1473515); /* ratio exactly .45 */
    assert(wf_light_millilux(360,640,&mlux) && mlux==289176);  /* exactly .64 */
    mlux=77; assert(wf_light_millilux(150,850,&mlux) && mlux==0); /* exactly .85: Appendix ELSE */
    assert(wf_light_millilux(0,1,&mlux) && mlux==0); /* observed valid dark quantization */
    assert(wf_light_millilux(0,1000,&mlux) && mlux==0); /* estimate, not a physical darkness claim */
    assert(!wf_light_millilux(65535,1,&mlux) && !wf_light_millilux(1,65535,&mlux));
    assert(wf_light_millilux(65534,0,&mlux) && mlux==116276976); /* no signed overflow */
    assert(wf_auto_brightness_target(0)==10 && wf_auto_brightness_target(300000)==55);
    assert(wf_auto_brightness_target(20000000)==100 && wf_auto_brightness_target(UINT32_MAX)==100);
    uint8_t previous=10;
    for (uint32_t value=0;value<20001000;value+=1003) {
        uint8_t next=wf_auto_brightness_target(value);
        assert(next>=previous && next<=100); previous=next;
    }
    wf_auto_brightness_t state={0};
    assert(wf_auto_brightness_observe(&state,300000)==55);
    assert(wf_auto_brightness_observe(&state,290000)==55); /* deadband */
    assert(wf_auto_brightness_observe(&state,0)==55);
    assert(wf_auto_brightness_observe(&state,0)==55);
    assert(wf_auto_brightness_observe(&state,0)==50);
    assert(wf_auto_brightness_observe(&state,0)==45);
    assert(wf_auto_brightness_observe(&state,1000000)==70); /* immediate brighter */
    for (unsigned i=0;i<10;++i) {
        assert(wf_auto_brightness_observe(&state,0)==70);
        assert(wf_auto_brightness_observe(&state,1000000)==70); /* breaks consecutive dark frames */
    }
    return 0;
}
