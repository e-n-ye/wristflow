#include "countdown.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    wristflow_countdown_t task;
    wristflow_countdown_init(&task);
    assert(task.phase == WRISTFLOW_COUNTDOWN_IDLE);
    assert(!wristflow_countdown_start(&task, 0, 100));
    assert(!wristflow_countdown_start(&task, 86400, 100));
    assert(wristflow_countdown_start(&task, 60, 100));
    uint32_t start_generation = task.generation;
    assert(!wristflow_countdown_start(&task, 30, 200));
    assert(wristflow_countdown_remaining(&task, 1100) == 59000);
    assert(wristflow_countdown_pause(&task, 1600));
    assert(task.remaining_ms == 58500);
    assert(wristflow_countdown_remaining(&task, 100000) == 58500);
    assert(!wristflow_countdown_dispatch(&task, start_generation, 100000));
    assert(!wristflow_countdown_update(&task, 100000));
    assert(!wristflow_countdown_pause(&task, 100000));
    assert(wristflow_countdown_resume(&task, 100000));
    assert(!wristflow_countdown_resume(&task, 100000));
    assert(wristflow_countdown_remaining(&task, 158499) == 1);
    assert(wristflow_countdown_dispatch(&task, task.generation, 158500));
    assert(task.phase == WRISTFLOW_COUNTDOWN_EXPIRED);
    assert(wristflow_countdown_take_expiry(&task));
    assert(!wristflow_countdown_take_expiry(&task));
    assert(!wristflow_countdown_update(&task, 159000));
    assert(wristflow_countdown_repeat(&task, 160000));
    assert(task.duration_ms == 60000 && wristflow_countdown_remaining(&task, 160000) == 60000);
    uint32_t repeat_generation = task.generation;
    wristflow_countdown_cancel(&task);
    assert(task.phase == WRISTFLOW_COUNTDOWN_IDLE && !task.expiry_pending);
    assert(!wristflow_countdown_repeat(&task, 160000));
    assert(!wristflow_countdown_dispatch(&task, repeat_generation, 220000));

    /* A delayed scheduler observes one deadline, regardless of missed frames. */
    assert(wristflow_countdown_start(&task, 1, 300000));
    assert(!wristflow_countdown_update(&task, 300999));
    assert(!wristflow_countdown_pause(&task, 301000));
    assert(task.phase == WRISTFLOW_COUNTDOWN_EXPIRED && task.expiry_pending);
    assert(wristflow_countdown_take_expiry(&task));
    wristflow_countdown_cancel(&task);
    assert(wristflow_countdown_start(&task, 1, 400000));
    assert(wristflow_countdown_update(&task, 500000));
    assert(!wristflow_countdown_update(&task, 600000));
    wristflow_countdown_cancel(&task);

    /* Wrap, maximum duration and resume across the wrap use modular time. */
    uint32_t origin = UINT32_MAX - 500;
    assert(wristflow_countdown_start(&task, 1, origin));
    assert(wristflow_countdown_remaining(&task, origin + 999U) == 1);
    assert(wristflow_countdown_update(&task, origin + 1000U));
    wristflow_countdown_cancel(&task);
    assert(wristflow_countdown_start(&task, WRISTFLOW_COUNTDOWN_MAX_SECONDS, origin));
    assert(wristflow_countdown_pause(&task, origin + 250U));
    assert(wristflow_countdown_resume(&task, 500));
    assert(wristflow_countdown_remaining(&task, 500 + 86398749U) == 1);
    assert(wristflow_countdown_update(&task, 500 + 86398750U));
    wristflow_countdown_init(&task); /* Reboot deliberately cancels the task. */
    assert(task.phase == WRISTFLOW_COUNTDOWN_IDLE && !task.expiry_pending);
    puts("countdown: boundaries, pause/resume, wrap and single expiry passed");
    return 0;
}
