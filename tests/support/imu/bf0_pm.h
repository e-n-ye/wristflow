#include <stdint.h>
#define AON_PIN_MODE_POS_EDGE 1
#define HPSYS_AON_WER_PIN0 0x100u
uint32_t test_wrist_wer(void);
#define HAL_HPAON_GET_WER() test_wrist_wer()
int8_t HAL_HPAON_QueryWakeupPin(void *, uint16_t);
int pm_enable_pin_wakeup(uint8_t, unsigned);
int pm_disable_pin_wakeup(uint8_t);
