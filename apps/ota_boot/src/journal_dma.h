#ifndef WF_BOOT_JOURNAL_DMA_H
#define WF_BOOT_JOURNAL_DMA_H

/* Called directly after the synchronous MPI2 journal program, with IRQs
 * masked and all DMA channels checked idle before the write. The SDK polling
 * path has released its software allocation by now. Match the completed
 * transfer before clearing the hardware EN bit left by that path. */
static inline bool wf_boot_finish_journal_dma(DMA_HandleTypeDef *dma, const void *src, int written)
{
    if ((hwp_mpi2->CR & MPI_CR_DMAE) || (hwp_mpi2->SR & MPI_SR_BUSY)) return false;
    uint32_t control = hwp_dmac1->CCR2;
    if (!(control & DMAC_CCR2_EN)) return true;

    const uint32_t port = (uint32_t)(uintptr_t)&hwp_mpi2->DR;
    if (written != WF_OTA_AB_BYTES || !dma || dma->Instance != FLASH2_DMA_INSTANCE ||
        dma->DmaBaseAddress != hwp_dmac1 || dma->State != HAL_DMA_STATE_READY ||
        dma->ErrorCode != HAL_DMA_ERROR_NONE || dma->LeftCounts || dma->TransCounts != WF_OTA_AB_BYTES ||
        dma->SrcAddress != (uint32_t)(uintptr_t)src + WF_OTA_AB_BYTES || dma->DstAddress != port ||
        hwp_dmac1->CPAR2 != port || (hwp_dmac1->ISR & DMAC_ISR_TEIF2) ||
        (hwp_dmac1->CNDTR2 & DMAC_CNDTR2_NDT) ||
        (control & ~DMAC_CCR2_PL_Msk) != (DMAC_CCR2_EN | DMAC_CCR2_DIR | DMAC_CCR2_MINC) ||
        ((hwp_dmac1->CSELR1 & DMAC_CSELR1_C2S) >> DMAC_CSELR1_C2S_Pos) != FLASH2_DMA_REQUEST)
        return false;

    __HAL_DMA_DISABLE(dma);
    __DSB();
    return !(hwp_dmac1->CCR2 & DMAC_CCR2_EN);
}
#endif
