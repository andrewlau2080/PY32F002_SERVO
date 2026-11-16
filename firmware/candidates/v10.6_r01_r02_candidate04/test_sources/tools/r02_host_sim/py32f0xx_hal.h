#ifndef R02_HOST_PY32F0XX_HAL_H
#define R02_HOST_PY32F0XX_HAL_H
#include <stdint.h>

typedef struct { uint32_t Pin, Mode, Pull, Speed, Alternate; } GPIO_InitTypeDef;
typedef struct { uint32_t unused; } GPIO_TypeDef;
typedef struct {
  uint32_t ClockPrescaler, Resolution, DataAlign, ScanConvMode, EOCSelection;
  uint32_t LowPowerAutoWait, ContinuousConvMode, DiscontinuousConvMode;
  uint32_t ExternalTrigConv, ExternalTrigConvEdge, Overrun, SamplingTimeCommon;
} ADC_InitTypeDef;
typedef struct { void *Instance; ADC_InitTypeDef Init; } ADC_HandleTypeDef;
typedef struct { uint32_t Rank, Channel; } ADC_ChannelConfTypeDef;

extern GPIO_TypeDef test_gpio;
#define GPIOA (&test_gpio)
#define GPIO_PIN_3 (1U << 3)
#define GPIO_PIN_4 (1U << 4)
#define GPIO_MODE_ANALOG 0U
#define GPIO_NOPULL 0U
#define ADC1 ((void *)1)
#define ADC_CHANNEL_3 3U
#define ADC_CHANNEL_4 4U
#define ADC_CHANNEL_VREFINT 16U
#define ADC_RANK_NONE 0U
#define ADC_RANK_CHANNEL_NUMBER 1U
#define ADC_CLOCK_SYNC_PCLK_DIV4 0U
#define ADC_RESOLUTION_12B 0U
#define ADC_DATAALIGN_RIGHT 0U
#define ADC_SCAN_DIRECTION_FORWARD 0U
#define ADC_EOC_SINGLE_CONV 0U
#define ADC_SOFTWARE_START 0U
#define ADC_EXTERNALTRIGCONVEDGE_NONE 0U
#define ADC_OVR_DATA_OVERWRITTEN 0U
#define ADC_SAMPLETIME_41CYCLES_5 0U
#define ENABLE 1U
#define DISABLE 0U
#define HAL_OK 0
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_ADC_FORCE_RESET() ((void)0)
#define __HAL_RCC_ADC_RELEASE_RESET() ((void)0)
#define __HAL_RCC_ADC_CLK_ENABLE() ((void)0)

int HAL_ADC_ConfigChannel(ADC_HandleTypeDef *, ADC_ChannelConfTypeDef *);
int HAL_ADC_Start(ADC_HandleTypeDef *);
int HAL_ADC_PollForConversion(ADC_HandleTypeDef *, uint32_t);
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *);
int HAL_ADC_Stop(ADC_HandleTypeDef *);
int HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *);
int HAL_ADC_Init(ADC_HandleTypeDef *);
void HAL_GPIO_Init(GPIO_TypeDef *, GPIO_InitTypeDef *);
uint32_t HAL_GetTick(void);
#endif
