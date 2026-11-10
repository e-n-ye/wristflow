/* USART1/PA19 via the existing CH340. Register setup follows the locked SDK
 * HAL_UART_Init/SetConfig; TX waits use a total poll budget, never HAL ticks. */
#include <bf0_hal.h>
#include "uart.h"
#ifdef WF_OTA_PROBE_UART_INIT
bool wf_ota_probe_uart_init(void)
{
    /* SF32LB52 has no USART1 enable-module enum. The official UART HAL
     * skips that operation under HPSYS_RCC_ENR1_USART1 on this chip. */
    HAL_PIN_Set(PAD_PA19,USART1_TXD,PIN_PULLUP,1);
    hwp_usart1->CR1=0;
    hwp_usart1->CR2=UART_STOPBITS_1;
    hwp_usart1->CR3=UART_HWCONTROL_NONE;
    hwp_usart1->CR1=UART_WORDLENGTH_8B|UART_MODE_TX|USART_CR1_UE;
    /* SDK requires BRR after UE and uses the fixed 48 MHz UART source. */
    MODIFY_REG(hwp_usart1->MISCR,USART_MISCR_SMPLINI_Msk,
               MAKE_REG_VAL(6,USART_MISCR_SMPLINI_Msk,USART_MISCR_SMPLINI_Pos));
    hwp_usart1->BRR=48;
    __DSB();
    return hwp_usart1->BRR==48 &&
        hwp_usart1->CR1==(UART_WORDLENGTH_8B|UART_MODE_TX|USART_CR1_UE) &&
        hwp_usart1->CR2==UART_STOPBITS_1 && hwp_usart1->CR3==UART_HWCONTROL_NONE;
}
#endif
static uint32_t flags(void *unused)
{
    (void)unused;
    uint32_t value=hwp_usart1->ISR;
    return ((value&UART_FLAG_TXE)?WF_OTA_PROBE_TXE:0u)|((value&UART_FLAG_TC)?WF_OTA_PROBE_TC:0u);
}
static void put(void *unused, uint8_t byte)
{
    (void)unused;
    hwp_usart1->TDR=byte;
}
bool wf_ota_probe_uart_frame(wf_ota_probe_frame_t *frame)
{
    if (hwp_usart1->BRR!=48 ||
        hwp_usart1->CR1!=(UART_WORDLENGTH_8B|UART_MODE_TX|USART_CR1_UE) ||
        hwp_usart1->CR2!=UART_STOPBITS_1 || hwp_usart1->CR3!=UART_HWCONTROL_NONE) return false;
    const wf_ota_probe_tx_t tx={flags,put,0};
    wf_ota_probe_frame_seal(frame);
    return wf_ota_probe_send(frame,&tx,2000000u);
}
