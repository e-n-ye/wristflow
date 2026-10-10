#ifndef WRISTFLOW_PRODUCT_PM_H
#define WRISTFLOW_PRODUCT_PM_H
#include <stdbool.h>
#include <rtthread.h>

#define WF_EVENT_KEY       1u
#define WF_EVENT_PHONE     2u
#define WF_EVENT_PM_SAMPLE 4u
#define WF_EVENT_TEST_WAKE 8u
#define WF_EVENT_COUNTDOWN 16u
#define WF_EVENT_ALL       31u

void wristflow_product_pm_start(void);
void wristflow_product_event_send(rt_uint32_t events);
rt_uint32_t wristflow_product_event_wait(rt_int32_t timeout);
void wristflow_product_pm_screen(bool off);
void wristflow_product_pm_phone_event(void);
void wristflow_product_pm_background(void);
void wristflow_product_pm_report(void);
#endif
