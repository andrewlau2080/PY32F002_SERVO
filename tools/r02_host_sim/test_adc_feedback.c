#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "adc_feedback.h"
#include "board.h"
#include "servo_params.h"

GPIO_TypeDef test_gpio;
static ServoParams params;
static uint32_t active_channel, tick, gpio_pin;
static uint16_t pot = 2000U;
static uint16_t vrefint = 1489U;
static uint16_t samples[4];
static unsigned sample_index, sample_count, failures;
static unsigned vref_reads;
static uint32_t fake_micros, vref_enable_micros;
static unsigned vref_settle_errors;
extern volatile uint32_t s_adc_raw_sample_ms;
extern volatile uint32_t s_vdd_sample_us_q4;
extern volatile uint32_t s_adc_raw_sample_us_q4;
extern volatile uint16_t g_adc_vdd_compensated_last;
extern volatile uint8_t s_vdd_sample_valid;

const ServoParams *Servo_Params_Get(void) { return &params; }
void APP_ErrorHandler(void) { fprintf(stderr, "ADC HAL failure\n"); exit(2); }
int HAL_ADC_ConfigChannel(ADC_HandleTypeDef *a, ADC_ChannelConfTypeDef *c)
{ (void)a; if (c->Rank != ADC_RANK_NONE) { active_channel = c->Channel; if (c->Channel == ADC_CHANNEL_VREFINT) vref_enable_micros = fake_micros; } return HAL_OK; }
int HAL_ADC_Start(ADC_HandleTypeDef *a)
{ (void)a; if ((active_channel == ADC_CHANNEL_VREFINT) && ((uint32_t)(fake_micros - vref_enable_micros) < 15U)) vref_settle_errors++; return HAL_OK; }
int HAL_ADC_PollForConversion(ADC_HandleTypeDef *a, uint32_t t)
{ (void)a; (void)t; return HAL_OK; }
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *a)
{
  (void)a;
  if (active_channel == ADC_CHANNEL_VREFINT) { vref_reads++; return vrefint; }
  if (active_channel != BOARD_POT_ADC_CHANNEL) APP_ErrorHandler();
  if (sample_index < sample_count) return samples[sample_index++];
  return pot;
}
int HAL_ADC_Stop(ADC_HandleTypeDef *a) { (void)a; return HAL_OK; }
int HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *a) { (void)a; return HAL_OK; }
int HAL_ADC_Init(ADC_HandleTypeDef *a) { (void)a; return HAL_OK; }
void HAL_GPIO_Init(GPIO_TypeDef *p, GPIO_InitTypeDef *g) { (void)p; gpio_pin = g->Pin; }
uint32_t HAL_GetTick(void) { return tick; }
uint32_t Board_Micros(void) { return fake_micros++; }
uint32_t Board_MicrosQ4(void) { return fake_micros++ * 16U; }

static void check(const char *name, int actual, int low, int high)
{
  int pass = actual >= low && actual <= high;
  printf("%s: %s (actual=%d expected=%d..%d)\n", pass ? "PASS" : "FAIL", name, actual, low, high);
  if (!pass) failures++;
}
static void run_updates(unsigned n)
{ for (unsigned i = 0; i < n; i++) { tick++; (void)ADC_Feedback_UpdateFiltered(); } }

int main(void)
{
  params.adc_sample_count = 4U;
  params.adc_filter_shift = 3U;
  ADC_Feedback_Init();
  check("configured physical ADC pin", gpio_pin, BOARD_POT_ADC_PIN, BOARD_POT_ADC_PIN);
  check("initial raw and filter", ADC_Feedback_GetFiltered(), 2000, 2000);
  check("initial VREF sample", ADC_Feedback_GetVrefintRaw(), 1489, 1489);
  check("position ADC sample timestamp", (int)s_adc_raw_sample_ms, (int)tick, (int)tick);
  check("position sample timestamp follows VDD sample",
        (int)(s_adc_raw_sample_us_q4 - s_vdd_sample_us_q4), 1, 0x7fffffff);
  check("VREFINT startup delay before first conversion", (int)vref_settle_errors, 0, 0);
  check("nominal VDD estimate", ADC_Feedback_GetVddMv(), 3300, 3301);

  pot = 2007U;
  run_updates(1U);
  check("positive step remains filtered", ADC_Feedback_GetFiltered(), 2000, 2001);
  run_updates(120U);
  check("positive 7-count step converges", ADC_Feedback_GetFiltered(), 2006, 2007);
  params.adc_filter_shift = 0U;
  run_updates(1U);
  params.adc_filter_shift = 3U;
  pot = 2000U;
  run_updates(120U);
  check("negative 7-count step converges", ADC_Feedback_GetFiltered(), 2000, 2001);
  pot = 2002U;
  run_updates(120U);
  check("positive 2-count step converges", ADC_Feedback_GetFiltered(), 2001, 2002);
  params.adc_filter_shift = 0U;
  run_updates(1U);
  params.adc_filter_shift = 3U;
  pot = 2000U;
  run_updates(120U);
  check("negative 2-count step converges", ADC_Feedback_GetFiltered(), 2000, 2001);

  params.adc_filter_shift = 0U;
  pot = 2100U;
  run_updates(1U);
  check("unfiltered setting responds immediately", ADC_Feedback_GetFiltered(), 2100, 2100);
  params.adc_filter_shift = 8U;
  pot = 2101U;
  run_updates(1U);
  check("strong filtering does not jump immediately", ADC_Feedback_GetFiltered(), 2100, 2100);
  run_updates(2048U);
  check("strong filtering converges to +1 count", ADC_Feedback_GetFiltered(), 2101, 2101);
  pot = 2100U;
  run_updates(2048U);
  check("strong filtering converges to -1 count", ADC_Feedback_GetFiltered(), 2100, 2100);

  samples[0] = 1900U; samples[1] = 2048U; samples[2] = 2049U; samples[3] = 2190U;
  sample_index = 0U; sample_count = 4U;
  check("four-sample trimmed mean", ADC_Feedback_ReadRaw(), 2048, 2048);
  samples[0] = 2207U; samples[1] = 2166U; samples[2] = 2166U; samples[3] = 2207U;
  sample_index = 0U; sample_count = 4U;
  check("two low samples retain half excursion after trimming",
        ADC_Feedback_ReadRaw(), 2186, 2187);
  samples[0] = 2166U; samples[1] = 2166U; samples[2] = 2166U; samples[3] = 2207U;
  sample_index = 0U; sample_count = 4U;
  check("three low samples retain full excursion after trimming",
        ADC_Feedback_ReadRaw(), 2166, 2166);
  sample_count = 0U;
  params.vdd_sample_interval_ms = 1U;
  run_updates(1U);
  check("periodic VREF sample", ADC_Feedback_GetVrefintRaw(), 1489, 1489);
  params.vdd_sample_interval_ms = 20U;
  vrefint = 1535U;
  tick += 19U;
  (void)ADC_Feedback_ReadRaw();
  check("VDD sample held inside interval", ADC_Feedback_GetVrefintRaw(), 1489, 1489);
  tick += 1U;
  (void)ADC_Feedback_ReadRaw();
  check("LCDM raw path refreshes VDD at interval", ADC_Feedback_GetVrefintRaw(), 1535, 1535);
  check("VREFINT startup delay before periodic conversion", (int)vref_settle_errors, 0, 0);
  check("VDD estimate follows VREF change", ADC_Feedback_GetVddMv(), 3201, 3201);
  check("valid VREF marks VDD sample usable", s_vdd_sample_valid, 1, 1);
  check("VDD sample timestamp", (int) ADC_Feedback_GetVddSampleMs(), (int)tick, (int)tick);
  check("VDD compensation normalizes to nominal rail",
        ADC_Feedback_ApplyVddCompensation(2000U, 3300U, 100U), 1940, 1940);
  check("VDD compensation telemetry matches control input",
        g_adc_vdd_compensated_last, 1940, 1940);
  check("VDD compensation saturates at ADC full scale",
        ADC_Feedback_ApplyVddCompensation(4095U, 3000U, 100U), 4095, 4095);
  check("nominal rail does not trip undervoltage guard",
        ADC_Feedback_IsVddBelow(3300U, 250U, 100U), 0, 0);
  tick += 101U;
  check("stale VDD falls back to raw ADC",
        ADC_Feedback_ApplyVddCompensation(2000U, 3300U, 100U), 2000, 2000);
  tick -= 101U;
  vrefint = 1700U;
  tick += 20U;
  (void)ADC_Feedback_ReadRaw();
  check("fresh VDD below configured threshold is detected",
        ADC_Feedback_IsVddBelow(3300U, 250U, 100U), 1, 1);
  vrefint = 1535U;
  tick += 20U;
  (void)ADC_Feedback_ReadRaw();
  check("recovered VDD clears undervoltage guard",
        ADC_Feedback_IsVddBelow(3300U, 250U, 100U), 0, 0);
  vrefint = 0U;
  tick += 20U;
  (void)ADC_Feedback_ReadRaw();
  check("invalid VREF keeps previous VDD estimate", ADC_Feedback_GetVddMv(), 3201, 3201);
  check("invalid VREF does not assert measured undervoltage",
        ADC_Feedback_IsVddBelow(3300U, 250U, 100U), 0, 0);
  check("invalid VREF disables VDD compensation",
        ADC_Feedback_ApplyVddCompensation(2000U, 3300U, 100U), 2000, 2000);
  check("invalid VREF marks sample unusable", s_vdd_sample_valid, 0, 0);
  unsigned reads_after_invalid = vref_reads;
  tick += 1U;
  (void)ADC_Feedback_ReadRaw();
  check("invalid VREF does not retrigger before interval", (int)vref_reads, (int)reads_after_invalid, (int)reads_after_invalid);
  pot = 0U;
  check("raw ADC supports lower rail", ADC_Feedback_ReadRaw(), 0, 0);
  pot = 4095U;
  check("raw ADC supports upper rail", ADC_Feedback_ReadRaw(), 4095, 4095);
  params.vdd_sample_interval_ms = 0U;
  params.adc_filter_shift = 0U;
  pot = 0U;
  run_updates(1U);
  params.adc_filter_shift = 3U;
  pot = 4095U;
  run_updates(300U);
  check("full-scale positive step converges", ADC_Feedback_GetFiltered(), 4095, 4095);
  pot = 0U;
  run_updates(300U);
  check("full-scale negative step converges", ADC_Feedback_GetFiltered(), 0, 0);
  return failures ? 1 : 0;
}
