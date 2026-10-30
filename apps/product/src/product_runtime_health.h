#ifndef WF_PRODUCT_RUNTIME_HEALTH_H
#define WF_PRODUCT_RUNTIME_HEALTH_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    bool imu_known,imu_fault,wrist_known,wrist_fault;
    uint32_t retries,recoveries;
} wf_product_runtime_health_t;
/* Pure publication copy; never probes or consumes a wake event. */
wf_product_runtime_health_t wf_product_runtime_health(void);
#endif
