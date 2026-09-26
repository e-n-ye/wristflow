#include "component_layout.h"
#include <assert.h>
#include <string.h>

static uint8_t disk[2][WRISTFLOW_LAYOUT_BYTES];
static unsigned limit = WRISTFLOW_LAYOUT_BYTES, last_key;
static bool readable = true;
static bool read_record(unsigned key, uint8_t *record, void *context)
{
    (void)context;
    if (!readable) return false;
    memcpy(record, disk[key], WRISTFLOW_LAYOUT_BYTES); return true;
}
static bool write_record(unsigned key, const uint8_t *record, void *context)
{
    (void)context; last_key = key;
    memset(disk[key], 0xff, WRISTFLOW_LAYOUT_BYTES);
    memcpy(disk[key], record, limit);
    return limit == WRISTFLOW_LAYOUT_BYTES;
}

int main(void)
{
    wristflow_layout_t layout = wristflow_layout_default(), next, restored;
    assert(wristflow_layout_valid(&layout) && layout.count == 3);
    assert(wristflow_layout_replace(&layout, 0, 1, "heart_rate", 1));
    assert(!strcmp(layout.pages[0].slots[0].app_id, layout.pages[0].slots[1].app_id));
    assert(layout.pages[0].slots[0].instance_id != layout.pages[0].slots[1].instance_id);
    for (unsigned t = 0; t < WRISTFLOW_LAYOUT_COUNT; ++t) {
        wristflow_layout_page_t page = {.template_id = (wristflow_template_t)t};
        for (unsigned s = 0; s < wristflow_template_slots(t); ++s) {
            assert(!wristflow_layout_page_complete(&page));
            strcpy(page.slots[s].app_id, "weather"); page.slots[s].size = wristflow_template_size(t, s);
            page.slots[s].variant = s % 2;
        }
        assert(wristflow_layout_page_complete(&page));
        next = layout;
        assert(wristflow_layout_insert(&next, 0, &page) && next.count == 4);
        assert(wristflow_layout_valid(&next));
        assert(wristflow_layout_insert(&next, next.count, &page));
        assert(wristflow_layout_insert(&next, 2, &page));
        wristflow_layout_t six = next;
        assert(!wristflow_layout_insert(&next, 0, &page) && !memcmp(&next, &six, sizeof next));
        while (next.count > 1) assert(wristflow_layout_delete(&next, 0));
        assert(!wristflow_layout_delete(&next, 0));
    }
    uint8_t record[WRISTFLOW_LAYOUT_BYTES]; uint32_t generation;
    assert(wristflow_layout_encode(&layout, 123, record));
    assert(wristflow_layout_decode(&next, &generation, record, sizeof record));
    assert(generation == 123 && !memcmp(&layout, &next, sizeof layout));
    for (unsigned i = 0; i < sizeof record; ++i) {
        record[i] ^= 1;
        assert(!wristflow_layout_decode(&next, &generation, record, sizeof record));
        record[i] ^= 1;
    }
    next.pages[0].slots[1].instance_id = next.pages[0].slots[0].instance_id;
    assert(!wristflow_layout_valid(&next));

    memset(disk, 0xff, sizeof disk);
    wristflow_layout_store_t store;
    restored = wristflow_layout_default();
    assert(!wristflow_layout_restore(&store, &restored, read_record, NULL));
    assert(wristflow_layout_commit(&store, &layout, read_record, write_record, NULL));
    assert(store.active == 0 && store.generation == 1);
    next = layout; assert(wristflow_layout_delete(&next, 1));
    uint8_t baseline[sizeof disk]; memcpy(baseline, disk, sizeof disk);
    /* Interrupt every byte boundary of the inactive record, then simulate reboot. */
    for (limit = 0; limit < WRISTFLOW_LAYOUT_BYTES; ++limit) {
        memcpy(disk, baseline, sizeof disk);
        store = (wristflow_layout_store_t){0, 1};
        assert(!wristflow_layout_commit(&store, &next, read_record, write_record, NULL));
        assert(last_key == 1 && !memcmp(disk[0], baseline, WRISTFLOW_LAYOUT_BYTES));
        assert(wristflow_layout_restore(&store, &restored, read_record, NULL));
        assert(!memcmp(&restored, &layout, sizeof layout) || !memcmp(&restored, &next, sizeof next));
    }
    limit = WRISTFLOW_LAYOUT_BYTES;
    assert(wristflow_layout_restore(&store, &restored, read_record, NULL));
    assert(wristflow_layout_commit(&store, &next, read_record, write_record, NULL));
    assert(wristflow_layout_restore(&store, &restored, read_record, NULL));
    assert(!memcmp(&next, &restored, sizeof next));
    readable = false;
    unsigned active = store.active;
    assert(!wristflow_layout_commit(&store, &layout, read_record, write_record, NULL));
    assert(store.active == (int)active);
    readable = true;
    /* Generation wrap still selects the newest complete record. */
    assert(wristflow_layout_encode(&layout, UINT32_MAX, disk[0]));
    assert(wristflow_layout_encode(&next, 0, disk[1]));
    assert(wristflow_layout_restore(&store, &restored, read_record, NULL));
    assert(store.active == 1 && !memcmp(&restored, &next, sizeof next));
    return 0;
}
