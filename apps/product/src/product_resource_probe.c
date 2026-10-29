#ifdef WF_RESOURCE_PROBE
#include "product_resource_probe.h"
#include "resource_probe.h"
#include "product_pm.h"
#include <rtthread.h>
#include <rthw.h>
#include <string.h>
#ifdef RT_USING_FINSH
#include <finsh.h>
#endif

/* FinSH exchanges only this IRQ-protected mailbox/cache with the UI thread. */
static wf_resource_probe_t probe, cache;
static wristflow_ui_shell_t *shell;
static lv_indev_t *pointer;
static const bool *panel_reserved;
static bool available, pending, stop;
static unsigned mode;
static uint32_t tag;

void wf_product_resource_init(wristflow_ui_shell_t *s, lv_indev_t *p, const bool *r)
{ shell = s; pointer = p; panel_reserved = r; }

bool wf_product_resource_busy_locked(void) { return pending || cache.active; }

static bool eligible(void)
{
    wristflow_carousel_snapshot_t s;
    return wristflow_ui_shell_carousel_snapshot(shell, &s) && s.settled &&
        s.surface == WRISTFLOW_SURFACE_HOME && s.page == 0 &&
        s.display_phase == WRISTFLOW_DISPLAY_ACTIVE &&
        !wristflow_ui_shell_endurance(shell) && !wristflow_ui_shell_charging_active(shell) &&
        !wristflow_ui_shell_reminder_active(shell) && !wristflow_ui_shell_notification_active(shell);
}

static void clean_pointer(void)
{ lv_indev_reset(pointer, NULL); lv_indev_wait_release(pointer); }

void wf_product_resource_poll(void)
{
    rt_base_t level = rt_hw_interrupt_disable();
    bool request = pending, cancel = stop, was_active = cache.active;
    unsigned requested_mode = mode;
    uint32_t requested_tag = tag;
    bool panel_busy = *panel_reserved;
    stop = false;
    rt_hw_interrupt_enable(level);
    bool allowed = !panel_busy && eligible();
    if (cancel) wf_resource_probe_stop(&probe, 2);
    if (request && (cancel || !allowed || !wf_resource_probe_start(&probe, shell, requested_mode, requested_tag)))
        probe = (wf_resource_probe_t){.tag=requested_tag, .mode=requested_mode, .reason=cancel ? 2 : 3};
    wf_resource_probe_tick(&probe, allowed);
    if (was_active && !probe.active) clean_pointer();
    level = rt_hw_interrupt_disable();
    if (request) pending = false;
    cache = probe;
    available = allowed && !probe.active && !*panel_reserved;
    rt_hw_interrupt_enable(level);
}

uint32_t wf_product_resource_events(uint32_t events)
{
    if (!probe.active) return events;
    if (events & (WF_EVENT_KEY | WF_EVENT_TEST_WAKE)) {
        wf_resource_probe_stop(&probe, 2);
        events &= ~(WF_EVENT_KEY | WF_EVENT_TEST_WAKE);
    } else if (events & (WF_EVENT_PHONE | WF_EVENT_ALARM | WF_EVENT_COUNTDOWN | WF_EVENT_CHARGER))
        wf_resource_probe_stop(&probe, 4);
    if (!probe.active) clean_pointer();
    return events;
}

#ifdef RT_USING_FINSH
static uint32_t parse_tag(const char *text)
{
    uint32_t value = 0;
    if (!*text || strlen(text) > 9) return 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') return 0;
        value = value * 10 + (unsigned)(*text - '0');
    }
    return value;
}

static int wf_resource(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "status")) {
        rt_base_t level = rt_hw_interrupt_disable();
        wf_resource_probe_t p = cache;
        bool queued = pending, ready = available;
        rt_hw_interrupt_enable(level);
        rt_kprintf("[resource] tag=%lu mode=%u active=%u pending=%u available=%u start=%lu elapsed=%lu reason=%u\n",
            (unsigned long)p.tag, p.mode, p.active, queued, ready,
            (unsigned long)p.started, (unsigned long)p.elapsed, p.reason);
        return 0;
    }
    if (argc == 4 && !strcmp(argv[1], "start") &&
        ((strlen(argv[2]) == 1 && argv[2][0] >= '0' && argv[2][0] <= '9') ||
         (strlen(argv[2]) == 2 && argv[2][0] == '1' && argv[2][1] >= '0' && argv[2][1] <= '4'))) {
        uint32_t requested_tag = parse_tag(argv[3]);
        rt_base_t level = rt_hw_interrupt_disable();
        bool accepted = requested_tag && available && !pending && !cache.active && !*panel_reserved;
        if (accepted) {
            mode = (unsigned)(argv[2][0] - '0');
            if (argv[2][1]) mode = 10 + (unsigned)(argv[2][1] - '0');
            tag = requested_tag;
            pending = true; available = false; stop = false;
        }
        rt_hw_interrupt_enable(level);
        rt_kprintf("[resource] accepted=%u tag=%lu\n", accepted, (unsigned long)requested_tag);
        return accepted ? 0 : -1;
    }
    if (argc == 3 && !strcmp(argv[1], "stop")) {
        uint32_t requested_tag = parse_tag(argv[2]);
        rt_base_t level = rt_hw_interrupt_disable();
        bool owned = requested_tag && ((pending && tag == requested_tag) ||
            (cache.active && cache.tag == requested_tag));
        if (owned) stop = true;
        rt_hw_interrupt_enable(level);
        rt_kprintf("[resource] stop_owned=%u tag=%lu\n", owned, (unsigned long)requested_tag);
        return owned ? 0 : -1;
    }
    rt_kprintf("wf_resource status | start <0..14> <1..999999999> | stop <tag>; 60s diagnostic only\n");
    return -1;
}
MSH_CMD_EXPORT(wf_resource, Bounded resource display validation);
#endif
#endif
