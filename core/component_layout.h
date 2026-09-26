#ifndef WRISTFLOW_COMPONENT_LAYOUT_H
#define WRISTFLOW_COMPONENT_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define WRISTFLOW_LAYOUT_MAX_PAGES 6U
#define WRISTFLOW_LAYOUT_BYTES 512U
typedef enum { WRISTFLOW_CARD_QUARTER = 1, WRISTFLOW_CARD_HALF = 2, WRISTFLOW_CARD_FULL = 4 } wristflow_card_size_t;
typedef enum { WRISTFLOW_LAYOUT_QUARTERS, WRISTFLOW_LAYOUT_TOP_HALF,
    WRISTFLOW_LAYOUT_BOTTOM_HALF, WRISTFLOW_LAYOUT_FULL, WRISTFLOW_LAYOUT_COUNT } wristflow_template_t;
typedef struct {
    char app_id[16];
    uint16_t instance_id;
    wristflow_card_size_t size;
    uint8_t variant;
} wristflow_component_t;
typedef struct {
    wristflow_template_t template_id;
    wristflow_component_t slots[4];
} wristflow_layout_page_t;
typedef struct {
    unsigned count;
    wristflow_layout_page_t pages[WRISTFLOW_LAYOUT_MAX_PAGES];
} wristflow_layout_t;
typedef enum { WRISTFLOW_SAVE_IDLE, WRISTFLOW_SAVE_PENDING, WRISTFLOW_SAVE_DONE,
    WRISTFLOW_SAVE_FAILED } wristflow_save_state_t;

unsigned wristflow_template_slots(wristflow_template_t id);
wristflow_card_size_t wristflow_template_size(wristflow_template_t id, unsigned slot);
wristflow_layout_t wristflow_layout_default(void);
bool wristflow_layout_page_complete(const wristflow_layout_page_t *page);
/* Structural validation is independent of the product's application registry. */
bool wristflow_layout_valid(const wristflow_layout_t *layout);
bool wristflow_layout_replace(wristflow_layout_t *layout, unsigned page, unsigned slot,
    const char *app_id, uint8_t variant);
bool wristflow_layout_insert(wristflow_layout_t *layout, unsigned index, const wristflow_layout_page_t *page);
bool wristflow_layout_delete(wristflow_layout_t *layout, unsigned index);
bool wristflow_layout_encode(const wristflow_layout_t *layout, uint32_t generation, uint8_t *record);
bool wristflow_layout_decode(wristflow_layout_t *layout, uint32_t *generation, const uint8_t *record, size_t size);

/* Two independently checked records. A commit never overwrites the active key. */
typedef bool (*wristflow_layout_read_t)(unsigned key, uint8_t *record, void *context);
typedef bool (*wristflow_layout_write_t)(unsigned key, const uint8_t *record, void *context);
typedef struct { int active; uint32_t generation; } wristflow_layout_store_t;
bool wristflow_layout_restore(wristflow_layout_store_t *store, wristflow_layout_t *layout,
    wristflow_layout_read_t read, void *context);
bool wristflow_layout_commit(wristflow_layout_store_t *store, const wristflow_layout_t *layout,
    wristflow_layout_read_t read, wristflow_layout_write_t write, void *context);
#endif
