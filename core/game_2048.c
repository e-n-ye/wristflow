#include "game_2048.h"
#include <limits.h>
#include <string.h>

static uint32_t random_next(wristflow_2048_t *game)
{
    uint32_t x = game->random;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return game->random = x;
}

static uint8_t spawn(wristflow_2048_t *game)
{
    uint8_t empty[16], count = 0;
    for (unsigned i = 0; i < 16; ++i)
        if (!game->cells[i]) empty[count++] = (uint8_t)i;
    if (!count) return UINT8_MAX;
    uint8_t index = empty[random_next(game) % count];
    game->cells[index] = random_next(game) % 10 == 0 ? 2 : 1;
    return index;
}

bool wristflow_2048_can_move(const wristflow_2048_t *game)
{
    if (!game) return false;
    for (unsigned i = 0; i < 16; ++i) {
        uint8_t value = game->cells[i];
        if (!value) return true;
        if (value >= 31) continue; /* Keep shifts and tile values representable. */
        if (i % 4 < 3 && value == game->cells[i + 1]) return true;
        if (i < 12 && value == game->cells[i + 4]) return true;
    }
    return false;
}

void wristflow_2048_restart(wristflow_2048_t *game, uint32_t seed)
{
    if (!game) return;
    uint32_t best = game->best;
    memset(game, 0, sizeof *game);
    game->best = best;
    game->random = seed ? seed : UINT32_C(0x6d2b79f5);
    spawn(game);
    spawn(game);
}

static unsigned index_at(unsigned line, unsigned offset, wristflow_2048_direction_t direction)
{
    switch (direction) {
    case WF_2048_LEFT: return line * 4 + offset;
    case WF_2048_RIGHT: return line * 4 + 3 - offset;
    case WF_2048_UP: return offset * 4 + line;
    case WF_2048_DOWN: return (3 - offset) * 4 + line;
    }
    return 0;
}

bool wristflow_2048_move(wristflow_2048_t *game, wristflow_2048_direction_t direction,
    wristflow_2048_change_t *change)
{
    if (change) *change = (wristflow_2048_change_t){0, UINT8_MAX};
    if (!game || (unsigned)direction > WF_2048_DOWN || game->over ||
        (game->won && !game->continued)) return false;
    uint8_t before[16];
    memcpy(before, game->cells, sizeof before);
    uint16_t merged = 0;
    for (unsigned line = 0; line < 4; ++line) {
        uint8_t compact[4] = {0}, result[4] = {0};
        unsigned count = 0, out = 0;
        for (unsigned i = 0; i < 4; ++i) {
            uint8_t value = before[index_at(line, i, direction)];
            if (value) compact[count++] = value;
        }
        for (unsigned i = 0; i < count; ++i) {
            uint8_t value = compact[i];
            if (i + 1 < count && value < 31 && value == compact[i + 1]) {
                ++value;
                ++i;
                uint32_t points = UINT32_C(1) << value;
                game->score = UINT32_MAX - game->score < points ? UINT32_MAX : game->score + points;
                merged |= (uint16_t)(1U << index_at(line, out, direction));
                if (value >= 11) game->won = true;
            }
            result[out++] = value;
        }
        for (unsigned i = 0; i < 4; ++i) game->cells[index_at(line, i, direction)] = result[i];
    }
    if (memcmp(before, game->cells, sizeof before) == 0) return false;
    if (game->score > game->best) game->best = game->score;
    uint8_t spawned = spawn(game);
    game->over = !wristflow_2048_can_move(game);
    if (change) *change = (wristflow_2048_change_t){merged, spawned};
    return true;
}

void wristflow_2048_continue(wristflow_2048_t *game)
{ if (game && game->won) game->continued = true; }
