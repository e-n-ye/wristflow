#include <rtconfig.h>
#ifdef WF_OTA_ANDROID_RECEIVER
#include <rtthread.h>
#include <string.h>
#include "bf0_sibles_nvds.h"

/* Only the loader uses this volatile NVDS backend. The normal application's
 * pairing database and FlashDB recovery/format paths are never opened here. */
static uint8_t stack[SIFLI_NVDS_KEY_LEN_STACK], app[SIFLI_NVDS_KEY_LEN_APP];
static uint8_t cm[SIFLI_NVDS_KEY_LEN_CM], bt_cm[SIFLI_NVDS_KEY_LEN_BT_CM];
static struct {
    const char *key;
    uint8_t *buffer;
    size_t capacity, length;
} entries[] = {
    {"SIF_STACK", stack, sizeof stack, 0}, {"SIF_APP", app, sizeof app, 0},
    {"SIF_CM", cm, sizeof cm, 0}, {"SIF_BT_CM", bt_cm, sizeof bt_cm, 0}
};

uint8_t sifli_nvds_flash_adaptor_init(void) { return NVDS_OK; }

size_t sifli_nvds_flash_adaptor_read(const char *key, void *out, size_t size)
{
    if (!key || !out) return 0;
    size_t count = 0;
    rt_enter_critical();
    for (unsigned i = 0; i < sizeof entries / sizeof entries[0]; i++) {
        if (!strcmp(key, entries[i].key)) {
            count = entries[i].length < size ? entries[i].length : size;
            memcpy(out, entries[i].buffer, count);
            break;
        }
    }
    rt_exit_critical();
    return count;
}

uint8_t sifli_nvds_flash_adaptor_write(const char *key, const void *data, size_t size)
{
    if (!key || !data) return NVDS_FAIL;
    uint8_t result = NVDS_FAIL;
    rt_enter_critical();
    for (unsigned i = 0; i < sizeof entries / sizeof entries[0]; i++) {
        if (!strcmp(key, entries[i].key) && size <= entries[i].capacity) {
            memcpy(entries[i].buffer, data, size);
            entries[i].length = size;
            result = NVDS_OK;
            break;
        }
    }
    rt_exit_critical();
    return result;
}

uint8_t sifli_nvds_flash_adaptor_delete(const char *key)
{
    rt_enter_critical();
    for (unsigned i = 0; i < sizeof entries / sizeof entries[0]; i++) {
        if (!key || !strcmp(key, entries[i].key)) entries[i].length = 0;
    }
    rt_exit_critical();
    return NVDS_OK;
}
#endif
