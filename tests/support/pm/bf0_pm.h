#define PM_SLEEP_MODE_IDLE 1
#define PM_SLEEP_MODE_DEEP 3
#define RT_PM_ENTER_SLEEP 0
#define RT_PM_EXIT_SLEEP 1
void rt_pm_request(uint8_t);
void rt_pm_release(uint8_t);
void rt_pm_notify_set(void (*)(uint8_t, uint8_t, void *), void *);
uint8_t rt_pm_sleep_mode_state_get(uint8_t);
uint32_t pm_get_power_mode(void);
uint32_t pm_get_wakeup_src(void);
rt_err_t pm_enable_pin_wakeup(uint8_t, int);
