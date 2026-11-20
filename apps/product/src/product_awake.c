#include "product_awake.h"
#include "product_pm.h"
#include <rthw.h>
#include <string.h>
#ifdef RT_USING_FINSH
#include <finsh.h>
#endif

#define AWAKE_MAX_SECONDS 1200u
#define AWAKE_REQUEST_MS 1500u

typedef struct {
    uint32_t token;
    unsigned seconds;
    rt_tick_t queued_at;
    bool start;
} awake_request_t;

/* The shell only writes this mailbox. The UI thread owns the display lease. */
static awake_request_t pending;
static uint32_t owner_token, wake_ack;
static char owner;

static void reply(uint32_t token, const char *result)
{
    rt_kprintf("[wf-awake] token=%08lx result=%s\n", (unsigned long)token, result);
}

#ifdef RT_USING_FINSH
static uint32_t decode_token(const char *text)
{
    if (strlen(text) != 8) return 0;
    uint32_t token = 0;
    for (; *text; ++text) {
        unsigned digit;
        if (*text >= '0' && *text <= '9') digit = *text - '0';
        else if (*text >= 'a' && *text <= 'f') digit = *text - 'a' + 10;
        else return 0;
        token = (token << 4) | digit;
    }
    return token;
}

static unsigned decode_seconds(const char *text)
{
    unsigned seconds = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') return 0;
        seconds = seconds * 10 + (*text - '0');
        if (seconds > AWAKE_MAX_SECONDS) return 0;
    }
    return seconds;
}

static int wf_awake(int argc, char **argv)
{
    bool start = argc == 4 && !strcmp(argv[1], "start");
    bool stop = argc == 3 && !strcmp(argv[1], "stop");
    uint32_t token = start || stop ? decode_token(argv[2]) : 0;
    unsigned seconds = start ? decode_seconds(argv[3]) : 0;
    if (!token || (start && !seconds)) {
        rt_kprintf("wf_awake start <8 hex digits> <1..1200 seconds> | stop <8 hex digits>\n");
        return -RT_EINVAL;
    }
    rt_base_t level = rt_hw_interrupt_disable();
    bool busy = pending.token != 0;
    if (!busy) pending = (awake_request_t){token, seconds, rt_tick_get(), start};
    rt_hw_interrupt_enable(level);
    if (busy) reply(token, "busy");
    else wristflow_product_event_send(WF_EVENT_LAB_AWAKE);
    return RT_EOK;
}
MSH_CMD_EXPORT(wf_awake, Start or stop one temporary screen hold over UART);
#endif

static void expired(void *context)
{
    (void)context;
    owner_token = 0;
    wake_ack = 0;
}

bool wf_product_awake_apply(wristflow_ui_shell_t *shell, bool eligible, bool screen_off)
{
    rt_base_t level = rt_hw_interrupt_disable();
    awake_request_t request = pending;
    pending.token = 0;
    rt_hw_interrupt_enable(level);
    if (!request.token) return false;

    if (!request.start) {
        if (owner_token == request.token) {
            wristflow_ui_shell_release_awake(shell, &owner);
            expired(NULL);
            reply(request.token, "stopped");
        } else reply(request.token, owner_token ? "other_owner" : "inactive");
        return false;
    }
    if (rt_tick_get() - request.queued_at > rt_tick_from_millisecond(AWAKE_REQUEST_MS)) {
        reply(request.token, "expired");
        return false;
    }
    if (!eligible || owner_token || wristflow_ui_shell_endurance(shell) ||
        wristflow_ui_shell_reminder_active(shell) ||
        !wristflow_ui_shell_acquire_awake(shell, &owner, "serial-lab", request.seconds * 1000u, expired)) {
        reply(request.token, "busy");
        return false;
    }
    owner_token = request.token;
    if (screen_off) wake_ack = request.token;
    else reply(request.token, "active");
    return screen_off;
}

void wf_product_awake_screen_on(void)
{
    if (wake_ack) reply(wake_ack, "active");
    wake_ack = 0;
}
