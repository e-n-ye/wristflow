#include "wrist_pose.h"

unsigned wf_wrist_pose_classify(const int16_t axes[3])
{
    int64_t x = axes[0], y = axes[1], z = axes[2];
    int64_t magnitude2 = x*x + y*y + z*z;
    unsigned flags = 0;
    if (x < -9000) flags |= WF_WRIST_POSE_X;
    if (y > -8000 && y < 8000) flags |= WF_WRIST_POSE_Y;
    /* Tilt toward the viewer: >=39.2deg from flat in the XZ plane.
     * Keep the previous backward-facing X/Z limits beyond vertical. */
    if (z < 7000 && (z <= 0 ? 3*x*x >= 2*z*z : x < -12000))
        flags |= WF_WRIST_POSE_VIEW;
    /* Reject free fall and large transient accelerations, not motion intent.
     * 13000..20000 raw counts is approximately 0.79..1.22g. */
    if (magnitude2 >= 13000LL*13000 && magnitude2 <= 20000LL*20000)
        flags |= WF_WRIST_POSE_GRAVITY;
    return flags;
}
