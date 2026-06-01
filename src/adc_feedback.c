#include "adc_feedback.h"
#include "board.h"
#include "main.h"
#include "servo_config.h"
#include "servo_params.h"

#define ADC_VREFINT_MV 1200U

static ADC_HandleTypeDef s_adc;
static uint16_t s_last_raw;
static uint16_t s_filtered;
static uint16_t s_vrefint_raw;
static uint16_t s_vdd_mv;
static uint32_t s_last_vdd_sample_ms;
static uint8_t s_filter_seeded;

static void select_channel(uint32_t adc_channel)
{
  ADC_ChannelConfTypeDef channel = {0};

  channel.Rank = ADC_RANK_NONE;
  channel.Channel = BOARD_POT_ADC_CHANNEL;
  if (HAL_ADC_ConfigChannel(&s_adc, &channel) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  channel.Channel = ADC_CHANNEL_VREFINT;
  if (HAL_ADC_ConfigChannel(&s_adc, &channel) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  channel.Rank = ADC_RANK_CHANNEL_NUMBER;
  channel.Channel = adc_channel;
  if (HAL_ADC_ConfigChannel(&s_adc, &channel) != HAL_OK)
  {
    APP_ErrorHandler();
  }
}

static uint16_t read_selected_channel_once(void)
{
  if (HAL_ADC_Start(&s_adc) != HAL_OK)
  {
    APP_ErrorHandler();
  }
  if (HAL_ADC_PollForConversion(&s_adc, 1000U) != HAL_OK)
  {
    APP_ErrorHandler();
  }
  uint16_t value = (uint16_t)HAL_ADC_GetValue(&s_adc);
  (void)HAL_ADC_Stop(&s_adc);
  return value;
}

static void update_vdd_sample(void)
{
  select_channel(ADC_CHANNEL_VREFINT);
  s_vrefint_raw = read_selected_channel_once();
  if (s_vrefint_raw > 0U)
  {
    uint32_t mv = (ADC_VREFINT_MV * 4095UL) / s_vrefint_raw;
    s_vdd_mv = (mv > 65535UL) ? 65535U : (uint16_t)mv;
  }
}

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

  update_vdd_sample();
  s_last_vdd_sample_ms = HAL_GetTick();
  s_filtered = ADC_Feedback_ReadRaw();
  s_filter_seeded = 1U;
}

uint16_t ADC_Feedback_ReadRaw(void)
{
  const ServoParams *params = Servo_Params_Get();
  uint32_t sum = 0U;
  uint16_t sample_count = params->adc_sample_count;

  if (sample_count == 0U)
  {
    sample_count = 1U;
  }
  else if (sample_count > 32U)
  {
    sample_count = 32U;
  }

  select_channel(BOARD_POT_ADC_CHANNEL);
  /* 多次采样取平均，采样次数由参数区决定，便于按电位器噪声单独调试。 */
  for (uint32_t i = 0; i < sample_count; i++)
  {
    sum += read_selected_channel_once();
  }

  s_last_raw = (uint16_t)(sum / sample_count);
  return s_last_raw;
}

uint16_t ADC_Feedback_UpdateFiltered(void)
{
  const ServoParams *params = Servo_Params_Get();
  uint16_t raw = ADC_Feedback_ReadRaw();
  uint16_t jump_limit = params->adc_jump_limit_count;
  uint16_t filter_shift = params->adc_filter_shift;
  uint32_t now_ms = HAL_GetTick();
  uint16_t vdd_interval = params->vdd_sample_interval_ms;

  if ((vdd_interval > 0U) &&
      ((uint32_t)(now_ms - s_last_vdd_sample_ms) >= vdd_interval))
  {
    update_vdd_sample();
    s_last_vdd_sample_ms = now_ms;
  }

  if (s_filter_seeded == 0U)
  {
    s_filtered = raw;
    s_filter_seeded = 1U;
  }
  else
  {
    if (jump_limit > 0U)
    {
      uint32_t upper = (uint32_t)s_filtered + jump_limit;
      uint32_t lower = (s_filtered > jump_limit) ? ((uint32_t)s_filtered - jump_limit) : 0U;
      if (raw > upper)
      {
        raw = (upper > 4095U) ? 4095U : (uint16_t)upper;
      }
      else if (raw < lower)
      {
        raw = (uint16_t)lower;
      }
    }

    if (filter_shift > 8U)
    {
      filter_shift = 8U;
    }

    /* 一阶 IIR：响应速度和抗干扰由 Flash 参数调节。 */
    int32_t delta = (int32_t)raw - (int32_t)s_filtered;
    if (filter_shift == 0U)
    {
      s_filtered = raw;
    }
    else
    {
      s_filtered = (uint16_t)((int32_t)s_filtered + (delta >> filter_shift));
    }
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

uint16_t ADC_Feedback_GetVrefintRaw(void)
{
  return s_vrefint_raw;
}

uint16_t ADC_Feedback_GetVddMv(void)
{
  return s_vdd_mv;
}
