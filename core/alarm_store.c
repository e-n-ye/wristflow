#include "alarm_store.h"
#include <string.h>
static void put32(uint8_t *p, uint32_t v)
{ for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (i * 8)); }
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static uint32_t crc32(const uint8_t *p)
{
    uint32_t crc = UINT32_MAX;
    for (unsigned i = 0; i < WRISTFLOW_ALARM_BYTES - 4; ++i) {
        crc ^= p[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((0U - (crc & 1U)) & 0xedb88320U);
    }
    return ~crc;
}
bool wristflow_alarm_encode(const wristflow_alarm_data_t *data, uint32_t generation, uint8_t *record)
{
    if (!record || !wristflow_alarm_data_valid(data)) return false;
    memset(record, 0, WRISTFLOW_ALARM_BYTES);
    memcpy(record, "WFAL", 4); record[4] = 1; record[5] = data->alarms.count;
    put32(record + 8, generation); put32(record + 12, data->alarms.next_id);
    for (unsigned i = 0; i < data->alarms.count; ++i) {
        uint8_t *p = record + 16 + i * 16;
        const wristflow_alarm_t *alarm = &data->alarms.entries[i];
        put32(p, alarm->id); p[4] = alarm->hour; p[5] = alarm->minute; p[6] = alarm->weekdays; p[7] = alarm->enabled;
        put32(p + 8, data->dates[i].once_utc); put32(p + 12, data->dates[i].last_fired_day);
    }
    put32(record + WRISTFLOW_ALARM_BYTES - 4, crc32(record));
    return true;
}
bool wristflow_alarm_decode(wristflow_alarm_data_t *data, uint32_t *generation, const uint8_t *record, size_t size)
{
    if (!data || !generation || !record || size != WRISTFLOW_ALARM_BYTES || memcmp(record, "WFAL", 4) ||
        record[4] != 1 || record[5] > WRISTFLOW_ALARM_CAPACITY || record[6] || record[7] ||
        get32(record + size - 4) != crc32(record)) return false;
    wristflow_alarm_data_t candidate = {0};
    candidate.alarms.count = record[5]; candidate.alarms.next_id = get32(record + 12);
    for (unsigned i = 0; i < candidate.alarms.count; ++i) {
        const uint8_t *p = record + 16 + i * 16;
        if (p[7] > 1) return false;
        candidate.alarms.entries[i] = (wristflow_alarm_t){.id = get32(p), .hour = p[4], .minute = p[5], .weekdays = p[6], .enabled = p[7] != 0};
        candidate.dates[i] = (wristflow_alarm_date_t){get32(p + 8), get32(p + 12)};
    }
    for (unsigned i = 16 + candidate.alarms.count * 16; i < size - 4; ++i) if (record[i]) return false;
    if (!wristflow_alarm_data_valid(&candidate)) return false;
    *data = candidate; *generation = get32(record + 8); return true;
}
bool wristflow_alarm_restore(wristflow_alarm_store_t *store, wristflow_alarm_data_t *data,
    wristflow_alarm_read_t read, void *context)
{
    *store = (wristflow_alarm_store_t){.active = -1};
    uint8_t record[WRISTFLOW_ALARM_BYTES];
    for (unsigned i = 0; i < 2; ++i) {
        wristflow_alarm_data_t candidate; uint32_t generation;
        if (!read(i, record, context) || !wristflow_alarm_decode(&candidate, &generation, record, sizeof record)) continue;
        uint32_t payload_checksum = get32(record + sizeof record - 4);
        if (!read(i + 2, record, context) || memcmp(record, "WFAC", 4) || record[4] != 1 ||
            get32(record + 8) != generation || get32(record + 12) != payload_checksum ||
            get32(record + sizeof record - 4) != crc32(record)) continue;
        if (store->active < 0 || (int32_t)(generation - store->generation) > 0) {
            *data = candidate; store->active = (int)i; store->generation = generation;
        }
    }
    return store->active >= 0;
}
bool wristflow_alarm_commit(wristflow_alarm_store_t *store, const wristflow_alarm_data_t *data,
    wristflow_alarm_read_t read, wristflow_alarm_write_t write, void *context)
{
    wristflow_alarm_prepared_t prepared;
    return wristflow_alarm_prepare(store, data, read, write, context, &prepared) &&
        wristflow_alarm_publish(store, &prepared, write, context);
}
bool wristflow_alarm_prepare(const wristflow_alarm_store_t *store, const wristflow_alarm_data_t *data,
    wristflow_alarm_read_t read, wristflow_alarm_write_t write, void *context, wristflow_alarm_prepared_t *prepared)
{
    uint8_t record[WRISTFLOW_ALARM_BYTES], check[WRISTFLOW_ALARM_BYTES];
    unsigned target = store->active == 0 ? 1U : 0U;
    uint32_t generation = store->generation + 1;
    if (!wristflow_alarm_encode(data, generation, record) || !write(target, record, context) ||
        !read(target, check, context) || memcmp(record, check, sizeof record)) return false;
    *prepared = (wristflow_alarm_prepared_t){target, generation, get32(record + sizeof record - 4)};
    return true;
}
bool wristflow_alarm_publish(wristflow_alarm_store_t *store, const wristflow_alarm_prepared_t *prepared,
    wristflow_alarm_write_t write, void *context)
{
    if (prepared->target > 1 || prepared->target == (unsigned)store->active || prepared->generation != store->generation + 1) return false;
    uint8_t marker[WRISTFLOW_ALARM_BYTES] = {0};
    memcpy(marker, "WFAC", 4); marker[4] = 1;
    put32(marker + 8, prepared->generation); put32(marker + 12, prepared->checksum);
    put32(marker + sizeof marker - 4, crc32(marker));
    if (!write(prepared->target + 2, marker, context)) return false;
    store->active = (int)prepared->target; store->generation = prepared->generation; return true;
}
