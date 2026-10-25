#include "compass.h"
#include <math.h>

bool wf_compass_raw_heading(int32_t x, int32_t y, uint16_t *degrees)
{
    if (!degrees || (!x && !y)) return false;
    float angle = atan2f((float)x, (float)y) * (180.0f / 3.14159265358979323846f);
    if (angle < 0) angle += 360.0f;
    *degrees = (uint16_t)((unsigned)(angle + 0.5f) % 360U);
    return true;
}
