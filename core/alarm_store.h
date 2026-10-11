#ifndef WRISTFLOW_ALARM_STORE_H
#define WRISTFLOW_ALARM_STORE_H
#include "alarm_schedule.h"
#include <stddef.h>
#define WRISTFLOW_ALARM_BYTES 104U
typedef bool (*wristflow_alarm_read_t)(unsigned key, uint8_t *record, void *context);
typedef bool (*wristflow_alarm_write_t)(unsigned key, const uint8_t *record, void *context);
typedef struct { int active; uint32_t generation; } wristflow_alarm_store_t;
typedef struct { unsigned target; uint32_t generation, checksum; } wristflow_alarm_prepared_t;
bool wristflow_alarm_encode(const wristflow_alarm_data_t *data, uint32_t generation, uint8_t *record);
bool wristflow_alarm_decode(wristflow_alarm_data_t *data, uint32_t *generation, const uint8_t *record, size_t size);
bool wristflow_alarm_restore(wristflow_alarm_store_t *store, wristflow_alarm_data_t *data,
    wristflow_alarm_read_t read, void *context);
bool wristflow_alarm_commit(wristflow_alarm_store_t *store, const wristflow_alarm_data_t *data,
    wristflow_alarm_read_t read, wristflow_alarm_write_t write, void *context);
/* Keys 0/1 hold payloads; keys 2/3 are publication markers. Preparation writes
 * and verifies only the inactive payload. A failed/stale preparation is never
 * selected at boot. Publish is the transaction's short final serialization point;
 * the owner must recheck its version before publishing. FlashDB must atomically
 * publish a key on a successful write, as its KV transaction contract specifies. */
bool wristflow_alarm_prepare(const wristflow_alarm_store_t *store, const wristflow_alarm_data_t *data,
    wristflow_alarm_read_t read, wristflow_alarm_write_t write, void *context, wristflow_alarm_prepared_t *prepared);
bool wristflow_alarm_publish(wristflow_alarm_store_t *store, const wristflow_alarm_prepared_t *prepared,
    wristflow_alarm_write_t write, void *context);
#endif
