#ifndef WF_PRODUCT_OTA_H
#define WF_PRODUCT_OTA_H
#include "ui_shell.h"
#ifdef WF_OTA_PRODUCT
void wf_product_ota_start(wristflow_ui_shell_t *shell, bool services_started);
void wf_product_ota_poll(wristflow_ui_shell_t *shell);
bool wf_product_ota_trial_pending(void);
#else
static inline void wf_product_ota_start(wristflow_ui_shell_t *shell, bool started) {(void)shell;(void)started;}
static inline void wf_product_ota_poll(wristflow_ui_shell_t *shell) {(void)shell;}
static inline bool wf_product_ota_trial_pending(void) {return false;}
#endif
#endif
