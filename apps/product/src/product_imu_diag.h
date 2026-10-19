#ifndef WRISTFLOW_PRODUCT_IMU_DIAG_H
#define WRISTFLOW_PRODUCT_IMU_DIAG_H
#include <stdbool.h>
/* Creates an idle command worker; no sensor I/O or sampling at startup. */
bool wristflow_product_imu_diag_start(void);
#endif
