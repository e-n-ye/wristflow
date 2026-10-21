#ifndef WRISTFLOW_PRODUCT_WRIST_IRQ_H
#define WRISTFLOW_PRODUCT_WRIST_IRQ_H
#include <stdbool.h>
#include <stdint.h>

typedef struct { bool armed, owned; uint32_t irqs; } wristflow_wrist_irq_snapshot_t;
/* Sole IMU-worker caller. Callback only signals that worker, never I2C/UI. */
bool wristflow_product_wrist_irq_start(void (*signal)(void));
bool wristflow_product_wrist_irq_stop(void);
wristflow_wrist_irq_snapshot_t wristflow_product_wrist_irq_snapshot(void);
#endif
