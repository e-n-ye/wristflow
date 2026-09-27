#define BSP_KEY1_PIN 34
#define GET_GPIO_INSTANCE(pin) 1
#define GET_GPIOx_PIN(pin) (pin)
#define AON_PIN_MODE_DOUBLE_EDGE 3
int8_t HAL_HPAON_QueryWakeupPin(int, int);
