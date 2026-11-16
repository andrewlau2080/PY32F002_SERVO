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
static uint16_t samples[4];
static unsigned sample_index, sample_count, failures;

const ServoParams *Servo_Params_Get(void) { return &params; }
void APP_ErrorHandler(void) { fprintf(stderr, "ADC HAL failure\n"); exit(2); }
int HAL_ADC_ConfigChannel(ADC_HandleTypeDef *a, ADC_ChannelConfTypeDef *c)
{ (void)a; if (c->Rank != ADC_RANK_NONE) active_channel = c->Channel; return HAL_OK; }
int HAL_ADC_Start(ADC_HandleTypeDef *a) { (void)a; return HAL_OK; }
int HAL_ADC_PollForConversion(ADC_HandleTypeDef *a, uint32_t t)
{ (void)a; (void)t; return HAL_OK; }
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *a)
{
  (void)a;
  if (active_channel == ADC_CHANNEL_VREFINT) return 1489U;
  if (active_channel != BOARD_POT_ADC_CHANNEL) APP_ErrorHandler();
  if (sample_index < sample_count) return samples[sample_index++];
  return pot;
}
int HAL_ADC_Stop(ADC_HandleTypeDef *a) { (void)a; return HAL_OK; }
int HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *a) { (void)a; return HAL_OK; }
int HAL_ADC_Init(ADC_HandleTypeDef *a) { (void)a; return HAL_OK; }
void HAL_GPIO_Init(GPIO_TypeDef *p, GPIO_InitTypeDef *g) { (void)p; gpio_pin = g->Pin; }
uint32_t HAL_GetTick(void) { return tick; }

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
  sample_count = 0U;
  params.vdd_sample_interval_ms = 1U;
  run_updates(1U);
  check("periodic VREF sample", ADC_Feedback_GetVrefintRaw(), 1489, 1489);
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
