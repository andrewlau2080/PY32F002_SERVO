#ifndef R01_HOST_PY32F0XX_HAL_H
#define R01_HOST_PY32F0XX_HAL_H

#include <stdint.h>

typedef struct { uint32_t Pin, Mode, Pull, Speed, Alternate; } GPIO_InitTypeDef;
typedef struct { uint32_t unused; } GPIO_TypeDef;
typedef struct {
  uint32_t CR1, DIER, PSC, ARR, CNT, CCMR1, CCER, EGR, SR, CCR1;
} R01TestTimer;

extern R01TestTimer r01_test_timer;
extern GPIO_TypeDef r01_test_gpio;
extern uint32_t SystemCoreClock;
#define TIM1 (&r01_test_timer)
#define GPIOA (&r01_test_gpio)
#define GPIO_PIN_3 (1U << 3)
#define GPIO_MODE_AF_PP 1U
#define GPIO_NOPULL 0U
#define GPIO_SPEED_FREQ_HIGH 1U
#define GPIO_AF13_TIM1 13U
#define TIM_CCMR1_CC1S_0 1U
#define TIM_CCER_CC1E 1U
#define TIM_CCER_CC1P 2U
#define TIM_EGR_UG 1U
#define TIM_DIER_CC1IE 2U
#define TIM_CR1_CEN 1U
#define TIM_SR_CC1IF 2U
#define TIM1_CC_IRQn 1
#define GPIO_PIN_SET 1U
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM1_CLK_ENABLE() ((void)0)
#define __disable_irq() ((void)0)
#define __enable_irq() ((void)0)

uint32_t HAL_GetTick(void);
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init);
uint32_t HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint32_t pin);
void HAL_NVIC_SetPriority(int irq, int priority, int subpriority);
void HAL_NVIC_ClearPendingIRQ(int irq);
void HAL_NVIC_EnableIRQ(int irq);

#endif
