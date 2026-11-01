#include "bf0_hal.h"
#define NOR_CMD_TABLE_CNT 2
#define FLASH_INVALID_ID 0
typedef struct {
    uint8_t manufacture_id,memory_type,memory_density,ext_flags;
    uint32_t mem_size;
} FLASH_RDID_TYPE_T;
extern const FLASH_RDID_TYPE_T *const flash_cmd_id_pool[];
extern const SPI_FLASH_FACT_CFG_T flash_cmd_table_list[];
