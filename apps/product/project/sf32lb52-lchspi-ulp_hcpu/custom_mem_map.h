/* Based on the locked board's Apache-2.0 custom map. Existing regions unchanged. */
#ifndef __CUSTOM_MEM_MAP__
#define __CUSTOM_MEM_MAP__
#ifdef USING_PARTITION_TABLE
#include "ptab.h"
#endif

#define FAL_PART_TABLE \
{ \
    {FAL_PART_MAGIC_WORD, "dfu", NOR_FLASH2_DEV_NAME, KVDB_DFU_REGION_OFFSET, KVDB_DFU_REGION_SIZE, 0}, \
    {FAL_PART_MAGIC_WORD, "ble", NOR_FLASH2_DEV_NAME, KVDB_BLE_REGION_OFFSET, KVDB_BLE_REGION_SIZE, 0}, \
    {FAL_PART_MAGIC_WORD, "settings", NOR_FLASH2_DEV_NAME, KVDB_SETTINGS_REGION_OFFSET, KVDB_SETTINGS_REGION_SIZE, 0}, \
}
#endif
