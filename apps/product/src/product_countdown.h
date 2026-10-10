#ifndef WRISTFLOW_PRODUCT_COUNTDOWN_H
#define WRISTFLOW_PRODUCT_COUNTDOWN_H
#include "countdown_port.h"
/* Start once at boot, after the PM event mailbox. No persistent task storage. */
const wristflow_countdown_port_t *wristflow_product_countdown_start(void);
#endif
