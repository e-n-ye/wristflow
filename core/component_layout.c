#include "component_layout.h"
#include <string.h>

unsigned wristflow_template_slots(wristflow_template_t id)
{
    static const unsigned counts[] = {4, 3, 3, 1};
    return (unsigned)id < WRISTFLOW_LAYOUT_COUNT ? counts[id] : 0;
}

wristflow_card_size_t wristflow_template_size(wristflow_template_t id, unsigned slot)
{
    if (slot >= wristflow_template_slots(id)) return 0;
    if (id == WRISTFLOW_LAYOUT_FULL) return WRISTFLOW_CARD_FULL;
    if ((id == WRISTFLOW_LAYOUT_TOP_HALF && slot == 0) ||
        (id == WRISTFLOW_LAYOUT_BOTTOM_HALF && slot == 2)) return WRISTFLOW_CARD_HALF;
    return WRISTFLOW_CARD_QUARTER;
}

wristflow_layout_t wristflow_layout_default(void)
{
    return (wristflow_layout_t){3, {
        {WRISTFLOW_LAYOUT_QUARTERS, {{"heart_rate",1,1,1}, {"blood_oxygen",2,1,0},
            {"battery",3,1,0}, {"stress",4,1,1}}},
        {WRISTFLOW_LAYOUT_TOP_HALF, {{"activity",5,2,0}, {"steps",6,1,0}, {"standing",7,1,0}}},
        {WRISTFLOW_LAYOUT_TOP_HALF, {{"weather",8,2,0}, {"sleep",9,1,1}, {"alarm",10,1,0}}}
    }};
}

static bool valid_app_id(const char *id)
{
    if (!id[0] || !memchr(id, 0, 16)) return false;
    for (unsigned i = 0; id[i]; ++i)
        if (!((id[i] >= 'a' && id[i] <= 'z') || id[i] == '_')) return false;
    return true;
}

bool wristflow_layout_page_complete(const wristflow_layout_page_t *page)
{
    if (!page || !wristflow_template_slots(page->template_id)) return false;
    for (unsigned i = 0; i < wristflow_template_slots(page->template_id); ++i) {
        const wristflow_component_t *slot = &page->slots[i];
        if (!valid_app_id(slot->app_id) || slot->variant > 1 ||
            slot->size != wristflow_template_size(page->template_id, i)) return false;
    }
    return true;
}

bool wristflow_layout_valid(const wristflow_layout_t *layout)
{
    if (!layout || !layout->count || layout->count > WRISTFLOW_LAYOUT_MAX_PAGES) return false;
    uint16_t ids[24]; unsigned count = 0;
    for (unsigned p = 0; p < layout->count; ++p) {
        const wristflow_layout_page_t *page = &layout->pages[p];
        if (!wristflow_layout_page_complete(page)) return false;
        for (unsigned s = 0; s < wristflow_template_slots(page->template_id); ++s) {
            uint16_t id = page->slots[s].instance_id;
            if (!id) return false;
            for (unsigned i = 0; i < count; ++i) if (ids[i] == id) return false;
            ids[count++] = id;
        }
    }
    return true;
}

static uint16_t free_id(const wristflow_layout_t *layout)
{
    /* Only 24 live instances; scan also handles reuse after a 16-bit wrap. */
    for (unsigned id = 1; id <= UINT16_MAX; ++id) {
        bool used = false;
        for (unsigned p = 0; p < layout->count; ++p)
            for (unsigned s = 0; s < wristflow_template_slots(layout->pages[p].template_id); ++s)
                if (layout->pages[p].slots[s].instance_id == id) used = true;
        if (!used) return (uint16_t)id;
    }
    return 0;
}

bool wristflow_layout_replace(wristflow_layout_t *layout, unsigned page, unsigned slot,
    const char *app_id, uint8_t variant)
{
    if (!wristflow_layout_valid(layout) || page >= layout->count ||
        slot >= wristflow_template_slots(layout->pages[page].template_id) ||
        !app_id || strlen(app_id) >= 16 || variant > 1) return false;
    wristflow_component_t next = {0};
    strcpy(next.app_id, app_id);
    if (!valid_app_id(next.app_id)) return false;
    next.size = wristflow_template_size(layout->pages[page].template_id, slot);
    next.variant = variant;
    next.instance_id = free_id(layout);
    layout->pages[page].slots[slot] = next;
    return true;
}

bool wristflow_layout_insert(wristflow_layout_t *layout, unsigned index, const wristflow_layout_page_t *page)
{
    if (!wristflow_layout_valid(layout) || layout->count >= WRISTFLOW_LAYOUT_MAX_PAGES ||
        index > layout->count || !wristflow_layout_page_complete(page)) return false;
    wristflow_layout_page_t copy = *page;
    for (unsigned i = 0; i < 4; ++i) copy.slots[i].instance_id = 0;
    memmove(&layout->pages[index + 1], &layout->pages[index], (layout->count - index) * sizeof copy);
    layout->pages[index] = copy;
    ++layout->count;
    for (unsigned s = 0; s < wristflow_template_slots(copy.template_id); ++s)
        layout->pages[index].slots[s].instance_id = free_id(layout);
    return true;
}

bool wristflow_layout_delete(wristflow_layout_t *layout, unsigned index)
{
    if (!wristflow_layout_valid(layout) || layout->count <= 1 || index >= layout->count) return false;
    --layout->count;
    memmove(&layout->pages[index], &layout->pages[index + 1], (layout->count - index) * sizeof layout->pages[0]);
    memset(&layout->pages[layout->count], 0, sizeof layout->pages[0]);
    return true;
}

static void put32(uint8_t *p, uint32_t value)
{ for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(value >> (8 * i)); }
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static uint32_t checksum(const uint8_t *p)
{
    uint32_t hash = 2166136261U;
    for (unsigned i = 0; i < WRISTFLOW_LAYOUT_BYTES - 4; ++i) hash = (hash ^ p[i]) * 16777619U;
    return hash;
}

bool wristflow_layout_encode(const wristflow_layout_t *layout, uint32_t generation, uint8_t *record)
{
    if (!record || !wristflow_layout_valid(layout)) return false;
    memset(record, 0, WRISTFLOW_LAYOUT_BYTES);
    memcpy(record, "WFLY", 4); record[4] = 1; record[5] = (uint8_t)layout->count;
    put32(record + 8, generation);
    for (unsigned p = 0; p < layout->count; ++p) {
        const wristflow_layout_page_t *page = &layout->pages[p];
        uint8_t *out = record + 12 + p * 82;
        out[0] = (uint8_t)page->template_id;
        out[1] = (uint8_t)wristflow_template_slots(page->template_id);
        for (unsigned s = 0; s < out[1]; ++s) {
            const wristflow_component_t *slot = &page->slots[s];
            uint8_t *item = out + 2 + s * 20;
            memcpy(item, slot->app_id, strlen(slot->app_id));
            item[16] = (uint8_t)slot->instance_id; item[17] = (uint8_t)(slot->instance_id >> 8);
            item[18] = (uint8_t)slot->size; item[19] = slot->variant;
        }
    }
    put32(record + WRISTFLOW_LAYOUT_BYTES - 4, checksum(record));
    return true;
}

bool wristflow_layout_decode(wristflow_layout_t *layout, uint32_t *generation, const uint8_t *record, size_t size)
{
    if (!layout || !generation || !record || size != WRISTFLOW_LAYOUT_BYTES ||
        memcmp(record, "WFLY", 4) || record[4] != 1 || record[5] > WRISTFLOW_LAYOUT_MAX_PAGES ||
        get32(record + size - 4) != checksum(record)) return false;
    wristflow_layout_t next = {0}; next.count = record[5];
    for (unsigned p = 0; p < next.count; ++p) {
        wristflow_layout_page_t *page = &next.pages[p];
        const uint8_t *in = record + 12 + p * 82;
        page->template_id = (wristflow_template_t)in[0];
        if (in[1] != wristflow_template_slots(page->template_id)) return false;
        for (unsigned s = 0; s < in[1]; ++s) {
            const uint8_t *item = in + 2 + s * 20;
            wristflow_component_t *slot = &page->slots[s];
            memcpy(slot->app_id, item, 16);
            slot->instance_id = (uint16_t)(item[16] | (uint16_t)item[17] << 8);
            slot->size = (wristflow_card_size_t)item[18]; slot->variant = item[19];
        }
    }
    if (!wristflow_layout_valid(&next)) return false;
    *layout = next; *generation = get32(record + 8);
    return true;
}

bool wristflow_layout_restore(wristflow_layout_store_t *store, wristflow_layout_t *layout,
    wristflow_layout_read_t read, void *context)
{
    *store = (wristflow_layout_store_t){.active = -1};
    uint8_t record[WRISTFLOW_LAYOUT_BYTES];
    for (unsigned i = 0; i < 2; ++i) {
        wristflow_layout_t candidate; uint32_t generation;
        if (!read(i, record, context) || !wristflow_layout_decode(&candidate, &generation, record, sizeof record)) continue;
        if (store->active < 0 || (int32_t)(generation - store->generation) > 0) {
            *layout = candidate; store->active = (int)i; store->generation = generation;
        }
    }
    return store->active >= 0;
}

bool wristflow_layout_commit(wristflow_layout_store_t *store, const wristflow_layout_t *layout,
    wristflow_layout_read_t read, wristflow_layout_write_t write, void *context)
{
    uint8_t record[WRISTFLOW_LAYOUT_BYTES], check[WRISTFLOW_LAYOUT_BYTES];
    uint32_t generation = store->generation + 1;
    unsigned target = store->active == 0 ? 1 : 0;
    if (!wristflow_layout_encode(layout, generation, record) || !write(target, record, context) ||
        !read(target, check, context) || memcmp(record, check, sizeof record)) return false;
    store->active = (int)target; store->generation = generation;
    return true;
}
