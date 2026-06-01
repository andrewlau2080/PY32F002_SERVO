#include "adc_feedback.h"
#include "board.h"
#include "main.h"
#include "servo_config.h"

static ADC_HandleTypeDef s_adc;
static uint16_t s_last_raw;
static uint16_t s_filtered;
static uint8_t s_filter_seeded;

void ADC_Feedback_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  ADC_ChannelConfTypeDef channel = {0};

  /* PA3 接电位器滑动端，必须配置成模拟输入以降低数字输入干扰。 */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_ADC_FORCE_RESET();
  __HAL_RCC_ADC_RELEASE_RESET();
  __HAL_RCC_ADC_CLK_ENABLE();

  gpio.Pin = BOARD_POT_ADC_PIN;
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BOARD_POT_ADC_PORT, &gpio);

  s_adc.Instance = ADC1;
  /* 上电校准 ADC，减少零点/增益误差对 1-3us 灵敏度的影响。 */
  if (HAL_ADCEx_Calibration_Start(&s_adc) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  s_adc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  s_adc.Init.Resolution = ADC_RESOLUTION_12B;
  s_adc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  s_adc.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
  s_adc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  s_adc.Init.LowPowerAutoWait = ENABLE;
  s_adc.Init.ContinuousConvMode = DISABLE;
  s_adc.Init.DiscontinuousConvMode = DISABLE;
  s_adc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  s_adc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  s_adc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  s_adc.Init.SamplingTimeCommon = ADC_SAMPLETIME_239CYCLES_5;

  if (HAL_ADC_Init(&s_adc) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  channel.Rank = ADC_RANK_CHANNEL_NUMBER;
  channel.Channel = BOARD_POT_ADC_CHANNEL;
  if (HAL_ADC_ConfigChannel(&s_adc, &channel) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  s_filtered = ADC_Feedback_ReadRaw();
  s_filter_seeded = 1U;
}

uint16_t ADC_Feedback_ReadRaw(void)
{
  uint32_t sum = 0U;

  /* 多次采样取平均，先降低随机噪声，再交给 IIR 滤波。 */
  for (uint32_t i = 0; i < SERVO_ADC_SAMPLE_COUNT; i++)
  {
    if (HAL_ADC_Start(&s_adc) != HAL_OK)
    {
      APP_ErrorHandler();
    }
    if (HAL_ADC_PollForConversion(&s_adc, 1000U) != HAL_OK)
    {
      APP_ErrorHandler();
    }
    sum += HAL_ADC_GetValue(&s_adc);
    (void)HAL_ADC_Stop(&s_adc);
  }

  s_last_raw = (uint16_t)(sum / SERVO_ADC_SAMPLE_COUNT);
  return s_last_raw;
}

uint16_t ADC_Feedback_UpdateFiltered(void)
{
  uint16_t raw = ADC_Feedback_ReadRaw();

  /* 一阶 IIR：响应速度和抗干扰由 SERVO_ADC_FILTER_SHIFT 调节。 */
  if (s_filter_seeded == 0U)
  {
    s_filtered = raw;
    s_filter_seeded = 1U;
  }
  else
  {
    int32_t delta = (int32_t)raw - (int32_t)s_filtered;
    s_filtered = (uint16_t)((int32_t)s_filtered + (delta >> SERVO_ADC_FILTER_SHIFT));
  }

  return s_filtered;
}

uint16_t ADC_Feedback_GetFiltered(void)
{
  return s_filtered;
}

uint16_t ADC_Feedback_GetRaw(void)
{
  return s_last_raw;
}
