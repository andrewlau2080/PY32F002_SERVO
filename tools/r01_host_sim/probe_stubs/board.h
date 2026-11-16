#include <stdint.h>
typedef struct { uint32_t MODER, OTYPER, IDR, ODR, BRR, AFR[2]; } GPIO_TypeDef;
typedef struct { uint32_t CR1, CR2, SMCR, DIER, CCER, BDTR, PSC, ARR, RCR, CCR2, CCMR1, EGR, SR, CNT; } TIM_TypeDef;
typedef struct { uint32_t APBENR2; } RCC_TypeDef;
typedef struct { uint32_t Pin, Mode, Pull, Speed; } GPIO_InitTypeDef;
typedef enum { GPIO_PIN_RESET=0, GPIO_PIN_SET=1 } GPIO_PinState;
extern GPIO_TypeDef gpioa, gpiob;
extern TIM_TypeDef tim1;
extern RCC_TypeDef rcc;
extern uint32_t SystemCoreClock;
#define GPIOA (&gpioa)
#define GPIOB (&gpiob)
#define TIM1 (&tim1)
#define RCC (&rcc)
#define BOARD_HB_IN1_PORT GPIOA
#define BOARD_HB_IN2_PORT GPIOA
#define BOARD_HB_IN3_PORT GPIOA
#define BOARD_HB_IN4_PORT GPIOB
#define BOARD_HB_IN1_PIN (1U << 1)
#define BOARD_HB_IN2_PIN (1U << 2)
#define BOARD_HB_IN3_PIN (1U << 6)
#define BOARD_HB_IN4_PIN 1U
#define BOARD_HB_IN3_ENABLE 1
#define BOARD_HB_IN4_ENABLE 1
#define GPIO_MODE_OUTPUT_PP 1
#define GPIO_PULLDOWN 2
#define GPIO_SPEED_FREQ_HIGH 3
#define RCC_APBENR2_TIM1EN (1U << 11)
#define TIM_EGR_UG 1U
#define TIM_CCER_CC2NE (1U << 6)
#define TIM_BDTR_MOE (1U << 15)
#define TIM_CR1_OPM (1U << 3)
#define TIM_CR1_CEN 1U
#define TIM_SR_UIF 1U
#define TIM_SR_CC2IF 4U
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM1_CLK_ENABLE() (rcc.APBENR2 |= RCC_APBENR2_TIM1EN)
#define __HAL_RCC_TIM1_CLK_DISABLE() (rcc.APBENR2 &= ~RCC_APBENR2_TIM1EN)
#define __disable_irq() ((void)0)
#define __enable_irq() ((void)0)
#define __get_PRIMASK() 0U
void HAL_GPIO_WritePin(GPIO_TypeDef *, uint16_t, GPIO_PinState);
void HAL_GPIO_Init(GPIO_TypeDef *, GPIO_InitTypeDef *);
