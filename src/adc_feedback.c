#include "adc_feedback.h"
#include "board.h"
#include "main.h"
#include "servo_config.h"
#include "servo_params.h"

#define ADC_VREFINT_MV 1200U
#define ADC_VREFINT_STARTUP_MAX_US 15U

#ifndef SERVO_ADC_SUBSAMPLE_DIAGNOSTIC
#define SERVO_ADC_SUBSAMPLE_DIAGNOSTIC 0
#endif

static ADC_HandleTypeDef s_adc;
static uint16_t s_last_raw;
static uint16_t s_filtered;
/* Q16 preserves progress even with the strongest (shift=8) filter setting. */
static int32_t s_filtered_q16;
/* Keep these symbols visible to the SWD monitor. VDD and position ADC are
 * sampled in the same ReadRaw() transaction at the configured cadence. */
volatile uint16_t s_vrefint_raw;
volatile uint16_t s_vdd_mv;
volatile uint8_t s_vdd_sample_valid;
volatile uint32_t s_vdd_sample_ms;
volatile uint32_t s_vdd_sample_us_q4;
volatile uint32_t s_adc_raw_sample_ms;
volatile uint32_t s_adc_raw_sample_us_q4;
volatile uint16_t g_adc_vdd_compensated_last;
static uint8_t s_filter_seeded;
#if SERVO_ADC_SUBSAMPLE_DIAGNOSTIC
/* Diagnostic-only last batch, exposed for a post-run SWD read. */
volatile uint16_t g_adc_diag_sample0;
volatile uint16_t g_adc_diag_sample1;
volatile uint16_t g_adc_diag_sample2;
volatile uint16_t g_adc_diag_sample3;
volatile uint16_t g_adc_diag_vdd0;
volatile uint16_t g_adc_diag_vdd1;
volatile uint16_t g_adc_diag_vdd2;
volatile uint16_t g_adc_diag_vdd3;
volatile uint32_t g_adc_diag_time0_q4;
volatile uint32_t g_adc_diag_time1_q4;
volatile uint32_t g_adc_diag_time2_q4;
volatile uint32_t g_adc_diag_time3_q4;
volatile uint32_t g_adc_diag_batch_count;
#endif

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
#if defined(SERVO_VREFINT_SETTLE_FIX) && (SERVO_VREFINT_SETTLE_FIX != 0)
  /* HAL enables the internal VREF path when the channel enters the sequence.
   * The PY32F002A datasheet allows up to 15 us startup; wait before sampling
   * so VDD telemetry does not depend on a partially settled reference. */
  uint32_t settle_start_us = Board_Micros();
  while ((uint32_t)(Board_Micros() - settle_start_us) < ADC_VREFINT_STARTUP_MAX_US)
  {
  }
#endif
  s_vrefint_raw = read_selected_channel_once();
  if ((s_vrefint_raw > 0U) && (s_vrefint_raw <= 4095U))
  {
    uint32_t mv = (ADC_VREFINT_MV * 4095UL) / s_vrefint_raw;
    s_vdd_mv = (mv > 65535UL) ? 65535U : (uint16_t)mv;
    s_vdd_sample_valid = 1U;
  }
  else
  {
    /* Keep the last display value, but never let it drive control correction. */
    s_vdd_sample_valid = 0U;
  }
  /* Timestamp even a zero/invalid conversion so a failed internal channel
   * cannot force an expensive retry before every position read. */
  s_vdd_sample_ms = HAL_GetTick();
  s_vdd_sample_us_q4 = Board_MicrosQ4();
}

static void update_vdd_if_due(uint32_t now_ms)
{
  const ServoParams *params = Servo_Params_Get();
  uint16_t interval_ms = params->vdd_sample_interval_ms;

  /* A zero/uninitialized interval must not silently disable supply tracking. */
  if (interval_ms == 0U)
  {
    interval_ms = 20U;
  }
  if ((uint32_t)(now_ms - s_vdd_sample_ms) >= interval_ms)
  {
    update_vdd_sample();
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
  s_adc.Init.SamplingTimeCommon = ADC_SAMPLETIME_41CYCLES_5;

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
  s_filtered = ADC_Feedback_ReadRaw();
  s_filtered_q16 = (int32_t)s_filtered * 65536;
  s_filter_seeded = 1U;
}

uint16_t ADC_Feedback_ReadRaw(void)
{
  const ServoParams *params = Servo_Params_Get();
  uint32_t sum = 0U;
  uint16_t sample_count = params->adc_sample_count;
  uint16_t min_sample = 0xFFFFU;
  uint16_t max_sample = 0U;

  /* LCDM uses this API directly. Sample VREFINT here too, so motor-load VDD
   * changes are observed even when the generic filtered path is not running. */
  update_vdd_if_due(HAL_GetTick());

  if (sample_count == 0U)
  {
    sample_count = 1U;
  }
  else if (sample_count > 32U)
  {
    sample_count = 32U;
  }

  select_channel(BOARD_POT_ADC_CHANNEL);
  /*
   * 多次采样去掉最大/最小值后平均，降低 H 桥开关尖峰和马达刷火噪声
   * 对电位器反馈的影响。采样次数仍由参数区决定，便于现场继续调试。
   */
  for (uint32_t i = 0; i < sample_count; i++)
  {
    uint16_t sample = read_selected_channel_once();
#if SERVO_ADC_SUBSAMPLE_DIAGNOSTIC
    if (sample_count == 4U)
    {
      uint32_t sample_time_q4 = Board_MicrosQ4();
      switch (i)
      {
        case 0U: g_adc_diag_sample0 = sample; g_adc_diag_vdd0 = s_vdd_mv; g_adc_diag_time0_q4 = sample_time_q4; break;
        case 1U: g_adc_diag_sample1 = sample; g_adc_diag_vdd1 = s_vdd_mv; g_adc_diag_time1_q4 = sample_time_q4; break;
        case 2U: g_adc_diag_sample2 = sample; g_adc_diag_vdd2 = s_vdd_mv; g_adc_diag_time2_q4 = sample_time_q4; break;
        case 3U: g_adc_diag_sample3 = sample; g_adc_diag_vdd3 = s_vdd_mv; g_adc_diag_time3_q4 = sample_time_q4; break;
        default: break;
      }
      if (i == 3U)
      {
        g_adc_diag_batch_count++;
      }
    }
#endif
    if (sample < min_sample)
    {
      min_sample = sample;
    }
    if (sample > max_sample)
    {
      max_sample = sample;
    }
    sum += sample;
  }

  if (sample_count >= 4U)
  {
    sum -= min_sample;
    sum -= max_sample;
    sample_count = (uint16_t)(sample_count - 2U);
  }

  s_last_raw = (uint16_t)(sum / sample_count);
  s_adc_raw_sample_ms = HAL_GetTick();
  s_adc_raw_sample_us_q4 = Board_MicrosQ4();
  return s_last_raw;
}

uint16_t ADC_Feedback_ReadFastRaw(void)
{
  s_last_raw = read_selected_channel_once();
  s_adc_raw_sample_ms = HAL_GetTick();
  s_adc_raw_sample_us_q4 = Board_MicrosQ4();
  return s_last_raw;
}

uint16_t ADC_Feedback_UpdateFiltered(void)
{
  const ServoParams *params = Servo_Params_Get();
  uint16_t raw = ADC_Feedback_ReadRaw();
  uint16_t jump_limit = params->adc_jump_limit_count;
  uint16_t filter_shift = params->adc_filter_shift;
  uint32_t now_ms = HAL_GetTick();
  update_vdd_if_due(now_ms);

  if (s_filter_seeded == 0U)
  {
    s_filtered = raw;
    s_filtered_q16 = (int32_t)raw * 65536;
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

    /* Q16 一阶 IIR 保留小于一个 ADC 计数的变化，避免整数截断产生死区。 */
    int32_t target_q16 = (int32_t)raw * 65536;
    if (filter_shift == 0U)
    {
      s_filtered_q16 = target_q16;
    }
    else
    {
      int32_t delta_q16 = target_q16 - s_filtered_q16;
      /* Signed division toward zero, using shifts on nonnegative values. */
      s_filtered_q16 += (delta_q16 >= 0) ? (delta_q16 >> filter_shift) :
                        -((-delta_q16) >> filter_shift);
    }
    s_filtered = (uint16_t)((s_filtered_q16 + 32768) / 65536);
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

uint32_t ADC_Feedback_GetVddSampleMs(void)
{
  return s_vdd_sample_ms;
}

uint16_t ADC_Feedback_ApplyVddCompensation(uint16_t raw_adc,
                                          uint16_t nominal_vdd_mv,
                                          uint16_t max_sample_age_ms)
{
  uint16_t vdd_mv = s_vdd_mv;
  uint32_t now_ms = HAL_GetTick();
  uint32_t age_ms = now_ms - s_vdd_sample_ms;
  uint32_t corrected;

  /* Use the uncorrected position if supply telemetry is absent or stale. */
  if ((raw_adc > 4095U) || (nominal_vdd_mv == 0U) || (vdd_mv == 0U) ||
      (s_vdd_sample_valid == 0U) ||
      (age_ms > max_sample_age_ms))
  {
    g_adc_vdd_compensated_last = raw_adc;
    return raw_adc;
  }

  corrected = (((uint32_t)raw_adc * (uint32_t)vdd_mv) +
               ((uint32_t)nominal_vdd_mv / 2U)) / (uint32_t)nominal_vdd_mv;
  if (corrected > 4095U)
  {
    corrected = 4095U;
  }
  g_adc_vdd_compensated_last = (uint16_t)corrected;
  return (uint16_t)corrected;
}

bool ADC_Feedback_IsVddBelow(uint16_t nominal_vdd_mv,
                             uint16_t warn_drop_mv,
                             uint16_t max_sample_age_ms)
{
  uint32_t age_ms = HAL_GetTick() - s_vdd_sample_ms;
  uint16_t minimum_vdd_mv;

  if ((s_vdd_sample_valid == 0U) || (s_vdd_mv == 0U) ||
      (nominal_vdd_mv == 0U) || (age_ms > max_sample_age_ms))
  {
    return false;
  }

  minimum_vdd_mv = (nominal_vdd_mv > warn_drop_mv) ?
                   (uint16_t)(nominal_vdd_mv - warn_drop_mv) : 0U;
  return s_vdd_mv < minimum_vdd_mv;
}
