#ifndef TEST_FS_HAL_H
#define TEST_FS_HAL_H
#include <stdint.h>
typedef struct { unsigned unused; } SPI_FLASH_FACT_CFG_T;
typedef struct {
    void *Instance;
    const SPI_FLASH_FACT_CFG_T *ctable;
    uint32_t base,size;
    uint8_t isNand,dualFlash;
} FLASH_HandleTypeDef;
#endif
