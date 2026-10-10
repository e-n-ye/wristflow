#include "countdown.h"
#include <string.h>

void wristflow_countdown_init(wristflow_countdown_t *task)
{
    if (task) memset(task, 0, sizeof(*task));
}

bool wristflow_countdown_start(wristflow_countdown_t *task, uint32_t seconds, uint32_t now)
{
    if (!task || !seconds || seconds > WRISTFLOW_COUNTDOWN_MAX_SECONDS ||
        task->phase == WRISTFLOW_COUNTDOWN_RUNNING || task->phase == WRISTFLOW_COUNTDOWN_PAUSED)
        return false;
    task->phase = WRISTFLOW_COUNTDOWN_RUNNING;
    task->duration_ms = task->remaining_ms = seconds * 1000U;
    task->anchor_ms = now;
    task->expiry_pending = false;
    ++task->generation;
    return true;
}

uint32_t wristflow_countdown_remaining(const wristflow_countdown_t *task, uint32_t now)
{
    if (!task) return 0;
    if (task->phase != WRISTFLOW_COUNTDOWN_RUNNING) return task->remaining_ms;
    uint32_t elapsed = now - task->anchor_ms;
    return elapsed >= task->remaining_ms ? 0 : task->remaining_ms - elapsed;
}

bool wristflow_countdown_update(wristflow_countdown_t *task, uint32_t now)
{
    if (!task || task->phase != WRISTFLOW_COUNTDOWN_RUNNING ||
        wristflow_countdown_remaining(task, now) != 0) return false;
    task->phase = WRISTFLOW_COUNTDOWN_EXPIRED;
    task->remaining_ms = 0;
    task->expiry_pending = true;
    return true;
}

bool wristflow_countdown_pause(wristflow_countdown_t *task, uint32_t now)
{
    if (!task || task->phase != WRISTFLOW_COUNTDOWN_RUNNING) return false;
    wristflow_countdown_update(task, now);
    if (task->phase != WRISTFLOW_COUNTDOWN_RUNNING) return false;
    task->remaining_ms = wristflow_countdown_remaining(task, now);
    task->phase = WRISTFLOW_COUNTDOWN_PAUSED;
    ++task->generation;
    return true;
}

bool wristflow_countdown_resume(wristflow_countdown_t *task, uint32_t now)
{
    if (!task || task->phase != WRISTFLOW_COUNTDOWN_PAUSED) return false;
    task->anchor_ms = now;
    task->phase = WRISTFLOW_COUNTDOWN_RUNNING;
    ++task->generation;
    return true;
}

void wristflow_countdown_cancel(wristflow_countdown_t *task)
{
    if (!task) return;
    uint32_t generation = task->generation + 1;
    wristflow_countdown_init(task);
    task->generation = generation;
}

bool wristflow_countdown_repeat(wristflow_countdown_t *task, uint32_t now)
{
    if (!task || task->phase != WRISTFLOW_COUNTDOWN_EXPIRED) return false;
    return wristflow_countdown_start(task, task->duration_ms / 1000U, now);
}

bool wristflow_countdown_dispatch(wristflow_countdown_t *task, uint32_t generation, uint32_t now)
{
    return task && task->generation == generation && wristflow_countdown_update(task, now);
}

bool wristflow_countdown_take_expiry(wristflow_countdown_t *task)
{
    if (!task || !task->expiry_pending) return false;
    task->expiry_pending = false;
    return true;
}
