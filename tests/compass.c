#ifdef NDEBUG
#undef NDEBUG
#endif
#include "compass.h"
#include <assert.h>
#include <limits.h>

int main(void)
{
    uint16_t result = 999;
    assert(!wf_compass_raw_heading(0, 0, &result) && result == 999);
    assert(!wf_compass_raw_heading(1, 1, 0));
    static const int32_t xy[][2] = {{0,1},{1,1},{1,0},{1,-1},{0,-1},{-1,-1},{-1,0},{-1,1}};
    for (unsigned i = 0; i < 8; ++i) {
        assert(wf_compass_raw_heading(xy[i][0], xy[i][1], &result) && result == i * 45);
        assert(wf_compass_raw_heading(xy[i][0] * 524287, xy[i][1] * 524287, &result) && result == i * 45);
    }
    assert(wf_compass_raw_heading(INT_MIN, INT_MAX, &result) && result == 315);
    assert(wf_compass_raw_heading(-1, 524287, &result) && result == 0);
    /* The retained five-window raw means rotate clockwise in this projection.
     * Values remain uncalibrated and are not expected to differ by 90 degrees. */
    assert(wf_compass_raw_heading(2372, 8312, &result) && result == 16);
    assert(wf_compass_raw_heading(4187, 2576, &result) && result == 58);
    return 0;
}
