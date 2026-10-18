#include "game_2048.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned at(unsigned i, unsigned direction)
{
    const unsigned indices[4][4] = {{0,1,2,3}, {3,2,1,0}, {0,4,8,12}, {12,8,4,0}};
    return indices[direction][i];
}
static uint64_t sum(const wristflow_2048_t *g)
{
    uint64_t n = 0;
    for (unsigned i = 0; i < 16; ++i) {
        assert(g->cells[i] <= 31);
        if (g->cells[i]) n += UINT64_C(1) << g->cells[i];
    }
    return n;
}
int main(void)
{
    wristflow_2048_t g = {0};
    wristflow_2048_restart(&g, 0);
    unsigned count = 0;
    for (unsigned i = 0; i < 16; ++i) if (g.cells[i]) { ++count; assert(g.cells[i] <= 2); }
    assert(count == 2 && g.random && g.score == 0);
    for (unsigned direction = 0; direction < 4; ++direction) {
        g = (wristflow_2048_t){.random=123};
        for (unsigned i = 0; i < 4; ++i) g.cells[at(i, direction)] = 1;
        wristflow_2048_change_t c;
        assert(wristflow_2048_move(&g, (wristflow_2048_direction_t)direction, &c));
        assert(g.cells[at(0,direction)] == 2 && g.cells[at(1,direction)] == 2);
        assert(g.score == 8 && g.best == 8);
        assert(c.merged == ((1U << at(0,direction)) | (1U << at(1,direction))));
        assert(c.spawned < 16 && g.cells[c.spawned] <= 2);
    }
    g = (wristflow_2048_t){.cells={1,1,2,0},.random=11};
    assert(wristflow_2048_move(&g, WF_2048_LEFT, NULL));
    assert(g.cells[0] == 2 && g.cells[1] == 2 && g.score == 4); /* No chain merge. */
    g = (wristflow_2048_t){.cells={0,1,0,1},.random=11};
    assert(wristflow_2048_move(&g, WF_2048_LEFT, NULL));
    assert(g.cells[0] == 2 && g.score == 4);
    g = (wristflow_2048_t){.cells={1,2,3,4},.random=11};
    wristflow_2048_t previous = g;
    assert(!wristflow_2048_move(&g, WF_2048_LEFT, NULL));
    assert(!memcmp(&previous, &g, sizeof g)); /* Invalid moves consume neither RNG nor score. */
    assert(!wristflow_2048_move(&g, (wristflow_2048_direction_t)99, NULL));
    assert(!wristflow_2048_move(&g, (wristflow_2048_direction_t)-1, NULL));
    assert(!memcmp(&previous, &g, sizeof g));
    g = (wristflow_2048_t){.cells={10,10},.random=11};
    assert(wristflow_2048_move(&g, WF_2048_LEFT, NULL));
    assert(g.won && !g.continued && g.cells[0] == 11 && g.score == 2048);
    previous = g;
    assert(!wristflow_2048_move(&g, WF_2048_DOWN, NULL));
    assert(!memcmp(&previous, &g, sizeof g));
    wristflow_2048_continue(&g);
    assert(wristflow_2048_move(&g, WF_2048_DOWN, NULL));
    uint32_t best = g.best;
    wristflow_2048_restart(&g, 42);
    assert(!g.won && !g.over && !g.continued && !g.score && g.best == best);
    g = (wristflow_2048_t){.cells={1,2,1,2, 2,1,2,1, 1,2,1,2, 2,1,2,1},.random=1};
    assert(!wristflow_2048_can_move(&g));
    g.cells[15] = 2;
    assert(wristflow_2048_can_move(&g));
    g = (wristflow_2048_t){.cells={30,30},.random=1,.score=UINT32_MAX-1};
    assert(wristflow_2048_move(&g, WF_2048_LEFT, NULL));
    assert(g.cells[0] == 31 && g.score == UINT32_MAX && g.best == UINT32_MAX);
    g = (wristflow_2048_t){.cells={31,31},.random=1,.continued=true};
    assert(!wristflow_2048_move(&g, WF_2048_LEFT, NULL));
    unsigned valid = 0, invalid = 0, losses = 0;
    for (unsigned seed = 1; seed <= 100; ++seed) {
        g = (wristflow_2048_t){0}; wristflow_2048_restart(&g, seed);
        for (unsigned turn = 0; turn < 500 && !g.over; ++turn) {
            if (g.won) wristflow_2048_continue(&g);
            previous = g;
            bool moved = wristflow_2048_move(&g, (wristflow_2048_direction_t)((seed*17+turn*7)%4), NULL);
            if (moved) {
                ++valid;
                uint64_t delta = sum(&g) - sum(&previous);
                assert(delta == 2 || delta == 4);
                assert(g.score >= previous.score && g.best >= g.score);
                assert(g.over == !wristflow_2048_can_move(&g));
            } else { ++invalid; assert(!memcmp(&g, &previous, sizeof g)); }
        }
        if (g.over) ++losses;
    }
    assert(valid && invalid && losses);
    printf("2048: rules, four directions, RNG, win/continue, saturation; %u valid / %u invalid / %u losses\n",valid,invalid,losses);
    return 0;
}
