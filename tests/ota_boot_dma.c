#include "ota_ab.h"
#include <assert.h>
#define __IO volatile
#include "dmac.h"
#include "mpi.h"

/* Host register storage. The firmware uses the SDK DMA handle and disable
 * macro; here we can also model a write that fails to clear EN. */
static DMAC_TypeDef controller;
static MPI_TypeDef mpi;
static uint8_t record[WF_OTA_AB_BYTES];
#define hwp_dmac1 (&controller)
#define hwp_mpi2 (&mpi)
#define FLASH2_DMA_INSTANCE (&controller.CCR2)
enum { FLASH2_DMA_REQUEST=1, HAL_DMA_STATE_READY=1, HAL_DMA_ERROR_NONE=0 };
typedef struct {
    volatile uint32_t *Instance;
    DMAC_TypeDef *DmaBaseAddress;
    uint32_t State, ErrorCode, LeftCounts, TransCounts, SrcAddress, DstAddress;
} DMA_HandleTypeDef;
static unsigned disables;
static bool stuck;
static void disable(DMA_HandleTypeDef *d)
{
    ++disables;
    if (!stuck) *d->Instance &= ~DMAC_CCR2_EN;
}
#define __HAL_DMA_DISABLE(d) disable(d)
#define __DSB() ((void)0)
#include "journal_dma.h"

static DMA_HandleTypeDef completed(void)
{
    controller = (DMAC_TypeDef){0};
    mpi = (MPI_TypeDef){0};
    mpi.CR = 1;
    controller.CCR2 = 4241;
    controller.CSELR1 = 256;
    controller.CPAR2 = (uint32_t)(uintptr_t)&mpi.DR;
    disables = 0;
    stuck = false;
    return (DMA_HandleTypeDef){FLASH2_DMA_INSTANCE,hwp_dmac1,HAL_DMA_STATE_READY,0,0,
        sizeof record,(uint32_t)(uintptr_t)(record+sizeof record),controller.CPAR2};
}

int main(void)
{
    DMA_HandleTypeDef d = completed();
    assert(wf_boot_finish_journal_dma(&d,record,sizeof record));
    assert(disables==1 && controller.CCR2==4240);
    assert(wf_boot_finish_journal_dma(&d,record,sizeof record) && disables==1);
    d = completed();
    stuck = true;
    assert(!wf_boot_finish_journal_dma(&d,record,sizeof record) && disables==1);
    for (unsigned i=0; i<16; ++i) {
        d = completed();
        int written = sizeof record;
        switch (i) {
        case 0: written--; break;
        case 1: mpi.CR |= MPI_CR_DMAE; break;
        case 2: mpi.SR |= MPI_SR_BUSY; break;
        case 3: controller.CNDTR2 = 1; break;
        case 4: controller.CSELR1 = 0; break;
        case 5: controller.CPAR2++; break;
        case 6: controller.ISR = DMAC_ISR_TEIF2; break;
        case 7: controller.CCR2 |= DMAC_CCR2_CIRC; break;
        case 8: d.Instance = &controller.CCR1; break;
        case 9: d.DmaBaseAddress = NULL; break;
        case 10: d.State = 0; break;
        case 11: d.ErrorCode = 1; break;
        case 12: d.LeftCounts = 1; break;
        case 13: d.TransCounts--; break;
        case 14: d.SrcAddress++; break;
        case 15: d.DstAddress++; break;
        }
        assert(!wf_boot_finish_journal_dma(&d,record,written));
        assert(disables==0 && (controller.CCR2 & DMAC_CCR2_EN));
    }
    return 0;
}
