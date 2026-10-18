#ifndef WRISTFLOW_GAME_2048_H
#define WRISTFLOW_GAME_2048_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { WF_2048_LEFT, WF_2048_RIGHT, WF_2048_UP, WF_2048_DOWN } wristflow_2048_direction_t;
typedef struct {
    /* Zero is empty; other cells contain the exponent of a power of two. */
    uint8_t cells[16];
    uint32_t score, best, random;
    bool won, continued, over;
} wristflow_2048_t;
typedef struct { uint16_t merged; uint8_t spawned; } wristflow_2048_change_t;

/* Zero-initialize once. Model/PRNG are caller-owned; restart preserves best. */
void wristflow_2048_restart(wristflow_2048_t *game, uint32_t seed);
bool wristflow_2048_move(wristflow_2048_t *game, wristflow_2048_direction_t direction,
    wristflow_2048_change_t *change);
bool wristflow_2048_can_move(const wristflow_2048_t *game);
void wristflow_2048_continue(wristflow_2048_t *game);
#endif
