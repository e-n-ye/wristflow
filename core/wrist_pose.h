#ifndef WRISTFLOW_WRIST_POSE_H
#define WRISTFLOW_WRIST_POSE_H
#include <stdint.h>

enum wf_wrist_pose_flags {
    WF_WRIST_POSE_X = 1, WF_WRIST_POSE_Y = 2,
    WF_WRIST_POSE_VIEW = 4, WF_WRIST_POSE_GRAVITY = 8,
    WF_WRIST_POSE_ACCEPT = 15
};
/* Product axis mapping, +/-2g raw counts (0.061mg/LSB). Pure classification;
 * baseline/departure, sample validity and time confirmation belong to the caller. */
unsigned wf_wrist_pose_classify(const int16_t axes[3]);
#endif
