#ifndef WRISTFLOW_PRODUCT_IMU_H
#define WRISTFLOW_PRODUCT_IMU_H
#include <stdbool.h>
void wristflow_product_imu_start(void);
/* UI-thread request: arm only when enabled and the display is dim or off. */
void wristflow_product_imu_wrist_wake(bool enabled);
#endif
