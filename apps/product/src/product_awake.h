#ifndef WRISTFLOW_PRODUCT_AWAKE_H
#define WRISTFLOW_PRODUCT_AWAKE_H

#include "ui_shell.h"

/* UI thread only. A successful start may need to wait for the first lit frame. */
bool wf_product_awake_apply(wristflow_ui_shell_t *shell, bool eligible, bool screen_off);
void wf_product_awake_screen_on(void);

#endif
