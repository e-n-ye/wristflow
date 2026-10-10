#ifndef WRISTFLOW_COUNTDOWN_PORT_H
#define WRISTFLOW_COUNTDOWN_PORT_H
#include "countdown.h"

typedef enum {
    WF_COUNTDOWN_START, WF_COUNTDOWN_PAUSE, WF_COUNTDOWN_RESUME,
    WF_COUNTDOWN_CANCEL, WF_COUNTDOWN_REPEAT, WF_COUNTDOWN_ACK, WF_COUNTDOWN_DISMISS
} wristflow_countdown_command_t;

/* Thread-safe service boundary. Read is a copy, not an expiry driver; now is
 * the same monotonic millisecond clock used by the service scheduler. */
typedef struct {
    void *context;
    bool (*command)(void *, wristflow_countdown_command_t, uint32_t seconds);
    void (*read)(void *, wristflow_countdown_t *);
    uint32_t (*now)(void *);
} wristflow_countdown_port_t;
#endif
