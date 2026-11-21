#include "tjc_lcdm.h"

#if SERVO_ENABLE_TJC_LCDM

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "main.h"
#include "servo_comm.h"
#include "servo_params.h"
#include "py32f0xx_hal_uart.h"

#ifndef SERVO_TJC_RX_ENABLE
#define SERVO_TJC_RX_ENABLE 1
#endif

#ifndef SERVO_TJC_VISUAL_TEST
#define SERVO_TJC_VISUAL_TEST 0
#endif

#ifndef SERVO_TJC_SOFT_TX
#define SERVO_TJC_SOFT_TX 0
#endif

#ifndef TJC_LCDM_VISUAL_TEST_MS
#define TJC_LCDM_VISUAL_TEST_MS 1000U
#endif

#ifndef TJC_LCDM_STARTUP_DELAY_MS
#define TJC_LCDM_STARTUP_DELAY_MS 1000U
#endif

UART_HandleTypeDef g_tjc_lcdm_uart;

static uint8_t s_rx_byte;
static uint8_t s_rx_packet[64];
static uint8_t s_rx_len;
static uint8_t s_rx_ff_count;
static volatile uint16_t s_touch_x;
static volatile uint16_t s_touch_y;
static volatile uint8_t s_touch_event;
static volatile uint8_t s_touch_seq;
static uint8_t s_touch_consumed_seq;
static uint8_t s_page;
static bool s_refresh_requested;
static bool s_home_num_valid[9];
static int32_t s_home_num_cache[9];
static bool s_home_text_valid[2];
static char s_home_text_cache[2][12];
static bool s_param_num_valid[10];
static int32_t s_param_num_cache[10];
static volatile uint32_t s_tjc_tx_cmd_count;
static volatile uint32_t s_tjc_tx_error_count;
static volatile uint32_t s_tjc_rx_byte_count;
static volatile uint32_t s_tjc_rx_packet_count;
static volatile uint32_t s_tjc_uart_error_count;
static volatile uint8_t s_tjc_last_packet_len;
static volatile uint8_t s_tjc_last_packet0;
static volatile uint8_t s_tjc_last_packet1;
static volatile uint8_t s_tjc_last_packet2;
static volatile uint8_t s_tjc_debug_page;
#if defined(SERVO_R01_PRODUCTION_LOOP_TEST) && SERVO_R01_PRODUCTION_LOOP_TEST
static volatile uint8_t s_r01_status_override_valid;
static volatile uint16_t s_r01_status_pulse_us;
static volatile uint16_t s_r01_status_feedback_adc;
static volatile uint16_t s_r01_status_target_adc;
static volatile int16_t s_r01_status_error_adc;
static void send_cmd(const char *cmd);

static void r01_draw_cmd(const char *cmd)
{
  send_cmd(cmd);
  HAL_Delay(2U);
}

static void r01_draw_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                          uint16_t color)
{
  char cmd[48];
  (void)snprintf(cmd, sizeof(cmd), "fill %u,%u,%u,%u,%u",
                 (unsigned)x, (unsigned)y, (unsigned)w, (unsigned)h,
                 (unsigned)color);
  r01_draw_cmd(cmd);
}

static void r01_draw_text(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                          uint16_t fg, uint16_t bg, uint8_t align,
                          const char *text)
{
  char cmd[128];
  (void)snprintf(cmd, sizeof(cmd),
                 "xstr %u,%u,%u,%u,0,%u,%u,%u,1,1,\"%s\"",
                 (unsigned)x, (unsigned)y, (unsigned)w, (unsigned)h,
                 (unsigned)fg, (unsigned)bg, (unsigned)align, text);
  r01_draw_cmd(cmd);
}

static void r01_draw_row(uint8_t row, const char *name, const char *value,
                         uint16_t value_color)
{
  static const uint16_t row_y[] = {35U, 62U, 89U, 116U, 145U, 173U, 201U, 229U};
  uint16_t bg = (row & 1U) ? 65535U : 61374U;
  uint16_t y = row_y[row];

  r01_draw_fill(12U, (uint16_t)(y - 2U), 300U, 26U, bg);
  r01_draw_text(20U, y, 135U, 24U, 31U, bg, 0U, name);
  r01_draw_text(165U, y, 140U, 24U, value_color, bg, 0U, value);
}

static void r01_draw_status_value(uint8_t row, int32_t number, uint16_t color)
{
  static int32_t last_number[8];
  static uint16_t last_color[8];
  static uint8_t valid_mask;
  static const uint16_t row_y[] = {35U, 62U, 89U, 116U, 145U, 173U, 201U, 229U};
  uint16_t bg = (row & 1U) ? 65535U : 61374U;
  uint8_t bit = (uint8_t)(1U << row);
  char value[24];

  if (((valid_mask & bit) != 0U) && (last_number[row] == number) &&
      (last_color[row] == color))
  {
    return;
  }

  last_number[row] = number;
  last_color[row] = color;
  valid_mask |= bit;
  if (row == 0U)
    (void)snprintf(value, sizeof(value), "%ld us", (long)number);
  else
    (void)snprintf(value, sizeof(value), "%ld", (long)number);
  r01_draw_text(165U, row_y[row], 140U, 24U, color, bg, 0U, value);
}

static void r01_draw_button(uint16_t y, const char *label, uint16_t color)
{
  r01_draw_fill(342U, y, 128U, 46U, color);
  r01_draw_text(350U, (uint16_t)(y + 8U), 112U, 30U, 65535U, color, 1U, label);
}

void TJC_LCDM_ShowR01Status(uint16_t pulse_us, uint16_t feedback_adc,
                            uint16_t target_adc, int16_t error_adc,
                            uint16_t pa6_duty, uint16_t pb0_duty,
                            uint16_t pa1_duty, uint16_t pa2_duty,
                            uint8_t motion_active)
{
  static bool page_drawn;
  static uint16_t last_pulse_us;
  static uint32_t last_draw_ms;
  static uint8_t previous_motion_active;
  static uint16_t peak_pa6_duty;
  static uint16_t peak_pb0_duty;
  static uint16_t peak_pa1_duty;
  static uint16_t peak_pa2_duty;
  uint32_t now = HAL_GetTick();

  if ((motion_active != 0U) && (previous_motion_active == 0U))
  {
    peak_pa6_duty = 0U;
    peak_pb0_duty = 0U;
    peak_pa1_duty = 0U;
    peak_pa2_duty = 0U;
  }
  previous_motion_active = motion_active;
  if (pa6_duty > peak_pa6_duty) peak_pa6_duty = pa6_duty;
  if (pb0_duty > peak_pb0_duty) peak_pb0_duty = pb0_duty;
  if (pa1_duty > peak_pa1_duty) peak_pa1_duty = pa1_duty;
  if (pa2_duty > peak_pa2_duty) peak_pa2_duty = pa2_duty;

  if ((pulse_us == last_pulse_us) && ((uint32_t)(now - last_draw_ms) < 250U))
  {
    return;
  }
  if (!page_drawn)
  {
    r01_draw_cmd("bkcmd=0");
    r01_draw_cmd("dim=100");
    r01_draw_cmd("page 0");
    HAL_Delay(250U);
    /* A page switch resumes refresh; stop it only after entering page 0. */
    r01_draw_cmd("ref_stop");
    /* Hide the panel's original widgets and disable their press/release
     * actions. Raw sendxy coordinates still drive our custom hit regions. */
    r01_draw_cmd("vis 255,0");
    r01_draw_cmd("tsw 255,0");
    r01_draw_cmd("sendxy=1");
    r01_draw_cmd("cls 65535");
    HAL_Delay(30U);
    r01_draw_fill(0U, 0U, 480U, 32U, 16U);
    r01_draw_text(6U, 3U, 340U, 26U, 65535U, 16U, 0U, "P50 PWM JOG");
    r01_draw_text(342U, 3U, 128U, 26U, 65535U, 16U, 2U, "PWM/1000");
    r01_draw_row(0U, "P", "-- us", 31U);
    r01_draw_row(1U, "T", "--", 31U);
    r01_draw_row(2U, "A", "--", 31U);
    r01_draw_row(3U, "E", "--", 63488U);
    r01_draw_row(4U, "PA6", "0", 65504U);
    r01_draw_row(5U, "PB0", "0", 65504U);
    r01_draw_row(6U, "PA1", "0", 65504U);
    r01_draw_row(7U, "PA2", "0", 65504U);
    r01_draw_button(58U, "FWD", 2016U);
    r01_draw_button(112U, "REV", 63488U);
    /* Commit the direct-draw page, then stop the original page widgets from
     * repainting their background over our runtime diagnostic UI. */
    r01_draw_cmd("ref_star");
    r01_draw_cmd("ref_stop");
    page_drawn = true;
  }

  last_pulse_us = pulse_us;
  last_draw_ms = now;
  r01_draw_status_value(0U, pulse_us, 31U);
  r01_draw_status_value(1U, target_adc, 31U);
  r01_draw_status_value(2U, feedback_adc, 31U);
  r01_draw_status_value(3U, error_adc, (error_adc < 0) ? 63488U : 31U);
  r01_draw_status_value(4U, peak_pa6_duty, (peak_pa6_duty != 0U) ? 31U : 65504U);
  r01_draw_status_value(5U, peak_pb0_duty, (peak_pb0_duty != 0U) ? 31U : 65504U);
  r01_draw_status_value(6U, peak_pa1_duty, (peak_pa1_duty != 0U) ? 63488U : 65504U);
  r01_draw_status_value(7U, peak_pa2_duty, (peak_pa2_duty != 0U) ? 63488U : 65504U);

  s_r01_status_pulse_us = pulse_us;
  s_r01_status_feedback_adc = feedback_adc;
  s_r01_status_target_adc = target_adc;
  s_r01_status_error_adc = error_adc;
  s_r01_status_override_valid = 1U;
}
#endif

#if SERVO_TJC_SOFT_TX
static uint32_t s_soft_tx_baud = TJC_LCDM_DEFAULT_BAUD;

static void soft_tx_wait_ticks(uint32_t ticks)
{
  uint32_t elapsed = 0U;
  uint32_t reload = SysTick->LOAD + 1U;
  uint32_t previous = SysTick->VAL;

  while (elapsed < ticks)
  {
    uint32_t current = SysTick->VAL;
    if (previous >= current)
    {
      elapsed += previous - current;
    }
    else
    {
      elapsed += previous + (reload - current);
    }
    previous = current;
  }
}

static void soft_tx_init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_7;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &gpio);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
}

static void soft_tx_byte(uint8_t value)
{
  uint32_t bit_ticks = SystemCoreClock / s_soft_tx_baud;

  GPIOA->BRR = GPIO_PIN_7;
  soft_tx_wait_ticks(bit_ticks);

  for (uint32_t i = 0; i < 8U; i++)
  {
    if ((value & (1U << i)) != 0U)
      GPIOA->BSRR = GPIO_PIN_7;
    else
      GPIOA->BRR = GPIO_PIN_7;
    soft_tx_wait_ticks(bit_ticks);
  }

  GPIOA->BSRR = GPIO_PIN_7;
  soft_tx_wait_ticks(bit_ticks);
}

#endif

static void send_end(void)
{
  static const uint8_t end_bytes[3] = {0xFFU, 0xFFU, 0xFFU};
#if SERVO_TJC_SOFT_TX
  for (uint32_t i = 0; i < sizeof(end_bytes); i++)
  {
    soft_tx_byte(end_bytes[i]);
  }
#else
  (void)HAL_UART_Transmit(&g_tjc_lcdm_uart, (uint8_t *)end_bytes, sizeof(end_bytes), 10U);
#endif
}

static void send_cmd(const char *cmd)
{
#if SERVO_TJC_SOFT_TX
  const uint8_t *bytes = (const uint8_t *)cmd;

  while (*bytes != 0U)
  {
    soft_tx_byte(*bytes++);
  }
  s_tjc_tx_cmd_count++;
#else
  HAL_StatusTypeDef status;

  status = HAL_UART_Transmit(&g_tjc_lcdm_uart, (uint8_t *)cmd, (uint16_t)strlen(cmd), 20U);
  if (status == HAL_OK)
  {
    s_tjc_tx_cmd_count++;
  }
  else
  {
    s_tjc_tx_error_count++;
  }
#endif
  send_end();
}

static void set_num(const char *obj, int32_t value)
{
  char cmd[32];
  (void)snprintf(cmd, sizeof(cmd), "%s.val=%ld", obj, (long)value);
  send_cmd(cmd);
}

static void set_text(const char *obj, const char *text)
{
  char cmd[48];
  (void)snprintf(cmd, sizeof(cmd), "%s.txt=\"%s\"", obj, text);
  send_cmd(cmd);
}

static void invalidate_page_cache(void)
{
  memset(s_home_num_valid, 0, sizeof(s_home_num_valid));
  memset(s_home_text_valid, 0, sizeof(s_home_text_valid));
  memset(s_param_num_valid, 0, sizeof(s_param_num_valid));
}

static void set_cached_num(const char *obj,
                           int32_t value,
                           bool *valid,
                           int32_t *cache,
                           uint8_t index)
{
  if (valid[index] && (cache[index] == value))
  {
    return;
  }

  cache[index] = value;
  valid[index] = true;
  set_num(obj, value);
}

static void set_cached_text(const char *obj,
                            const char *text,
                            bool *valid,
                            char cache[][12],
                            uint8_t index)
{
  if (valid[index] && (strncmp(cache[index], text, sizeof(cache[index])) == 0))
  {
    return;
  }

  (void)snprintf(cache[index], sizeof(cache[index]), "%s", text);
  valid[index] = true;
  set_text(obj, cache[index]);
}

static uint8_t scale_percent(uint16_t value, uint16_t min_value, uint16_t max_value)
{
  if (max_value <= min_value)
  {
    return 0U;
  }

  if (value <= min_value)
  {
    return 0U;
  }

  if (value >= max_value)
  {
    return 100U;
  }

  return (uint8_t)(((uint32_t)(value - min_value) * 100UL) / (uint32_t)(max_value - min_value));
}

static const char *state_text(uint8_t state)
{
  switch (state)
  {
    case 0U:
      return "NO SIG";
    case 1U:
      return "HOLD";
    case 2U:
      return "DRIVE";
    case 3U:
      return "STALL";
    case 4U:
      return "VDD LOW";
    default:
      return "UNKNOWN";
  }
}

static void send_home_page(void)
{
  ServoTelemetry telemetry;
  Servo_Comm_FillTelemetry(&telemetry);
#if defined(SERVO_R01_PRODUCTION_LOOP_TEST) && SERVO_R01_PRODUCTION_LOOP_TEST
  if (s_r01_status_override_valid != 0U)
  {
    telemetry.input_us = s_r01_status_pulse_us;
    telemetry.feedback_adc = s_r01_status_feedback_adc;
    telemetry.target_adc = s_r01_status_target_adc;
    telemetry.error_adc = s_r01_status_error_adc;
  }
#endif

  int16_t duty = telemetry.motor_duty;
  if (duty < 0)
  {
    duty = (int16_t)-duty;
  }

  uint8_t target_percent = scale_percent(telemetry.target_adc,
                                         telemetry.adc_min_count,
                                         telemetry.adc_max_count);
  uint8_t feedback_percent = scale_percent(telemetry.feedback_adc,
                                           telemetry.adc_min_count,
                                           telemetry.adc_max_count);
  uint8_t duty_percent = (uint8_t)(((uint32_t)duty * 100UL) / 1000UL);

  set_cached_num("n0", telemetry.input_us, s_home_num_valid, s_home_num_cache, 0U);
  set_cached_num("n1", telemetry.target_adc, s_home_num_valid, s_home_num_cache, 1U);
  set_cached_num("n2", telemetry.feedback_adc, s_home_num_valid, s_home_num_cache, 2U);
  set_cached_num("n3", telemetry.error_adc, s_home_num_valid, s_home_num_cache, 3U);
  set_cached_num("n4", telemetry.motor_duty, s_home_num_valid, s_home_num_cache, 4U);
  set_cached_num("n5", telemetry.vdd_mv, s_home_num_valid, s_home_num_cache, 5U);
  set_cached_num("j0", target_percent, s_home_num_valid, s_home_num_cache, 6U);
  set_cached_num("j1", feedback_percent, s_home_num_valid, s_home_num_cache, 7U);
  set_cached_num("j2", duty_percent, s_home_num_valid, s_home_num_cache, 8U);
  set_cached_text("t0", state_text(telemetry.servo_state), s_home_text_valid, s_home_text_cache, 0U);
  set_cached_text("t1", (telemetry.params_valid != 0U) ? "CRC OK" : "CRC BAD", s_home_text_valid, s_home_text_cache, 1U);
}

static void send_param_page(void)
{
  const ServoParams *params = Servo_Params_Get();

  set_cached_num("n10", params->pulse_lower_us, s_param_num_valid, s_param_num_cache, 0U);
  set_cached_num("n11", params->pulse_center_us, s_param_num_valid, s_param_num_cache, 1U);
  set_cached_num("n12", params->pulse_high_us, s_param_num_valid, s_param_num_cache, 2U);
  set_cached_num("n13", params->adc_min_count, s_param_num_valid, s_param_num_cache, 3U);
  set_cached_num("n14", params->adc_center_count, s_param_num_valid, s_param_num_cache, 4U);
  set_cached_num("n15", params->adc_max_count, s_param_num_valid, s_param_num_cache, 5U);
  set_cached_num("n16", params->deadband_us, s_param_num_valid, s_param_num_cache, 6U);
  set_cached_num("n17", params->deadband_count_min, s_param_num_valid, s_param_num_cache, 7U);
  set_cached_num("n18", params->max_duty, s_param_num_valid, s_param_num_cache, 8U);
  set_cached_num("n19", params->boost_duty, s_param_num_valid, s_param_num_cache, 9U);
}

static void process_packet(const uint8_t *packet, uint8_t len)
{
  if (len == 0U)
  {
    return;
  }

  s_tjc_rx_packet_count++;
  s_tjc_last_packet_len = len;
  s_tjc_last_packet0 = packet[0];
  s_tjc_last_packet1 = (len > 1U) ? packet[1] : 0U;
  s_tjc_last_packet2 = (len > 2U) ? packet[2] : 0U;

  if ((len >= 6U) && (packet[0] == 0x67U))
  {
    s_touch_x = (uint16_t)(((uint16_t)packet[1] << 8) | packet[2]);
    s_touch_y = (uint16_t)(((uint16_t)packet[3] << 8) | packet[4]);
    s_touch_event = packet[5];
    s_touch_seq++;
    return;
  }

  if ((len >= 3U) && (packet[0] == 0x65U))
  {
    if (s_page != packet[1])
    {
      invalidate_page_cache();
    }
    s_page = packet[1];
    s_tjc_debug_page = s_page;
    s_refresh_requested = true;
    return;
  }

  if ((len >= 6U) && (memcmp(packet, "page=", 5U) == 0))
  {
    uint8_t page = (uint8_t)(packet[5] - (uint8_t)'0');
    if (s_page != page)
    {
      invalidate_page_cache();
    }
    s_page = page;
    s_tjc_debug_page = s_page;
    s_refresh_requested = true;
    return;
  }

  if ((len >= 7U) && (memcmp(packet, "refresh", 7U) == 0))
  {
    invalidate_page_cache();
    s_refresh_requested = true;
  }
}

static void start_receive(void)
{
  (void)HAL_UART_Receive_IT(&g_tjc_lcdm_uart, &s_rx_byte, 1U);
}

void TJC_LCDM_Init(void)
{
#if SERVO_TJC_SOFT_TX
  soft_tx_init();
  s_soft_tx_baud = TJC_LCDM_DEFAULT_BAUD;
  /* Keep the panel's established 230400 baud; do not renegotiate it to 9600. */
  send_cmd("bkcmd=0");
  send_cmd("dim=100");
  s_page = 0U;
  s_tjc_debug_page = s_page;
  invalidate_page_cache();
  s_refresh_requested = true;
#else
  /* Let the panel finish its own power-up before the first UART command. */
  HAL_Delay(TJC_LCDM_STARTUP_DELAY_MS);
  g_tjc_lcdm_uart.Instance = USART1;
  g_tjc_lcdm_uart.Init.BaudRate = TJC_LCDM_DEFAULT_BAUD;
  g_tjc_lcdm_uart.Init.WordLength = UART_WORDLENGTH_8B;
  g_tjc_lcdm_uart.Init.StopBits = UART_STOPBITS_1;
  g_tjc_lcdm_uart.Init.Parity = UART_PARITY_NONE;
  g_tjc_lcdm_uart.Init.Mode = UART_MODE_TX_RX;
  g_tjc_lcdm_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  g_tjc_lcdm_uart.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&g_tjc_lcdm_uart) != HAL_OK)
  {
    APP_ErrorHandler();
  }

#if SERVO_TJC_RX_ENABLE
  send_cmd("sendxy=1");
#endif

  s_page = 0U;
  s_tjc_debug_page = s_page;
  invalidate_page_cache();
  s_refresh_requested = true;
#if SERVO_TJC_RX_ENABLE
  start_receive();
#endif
#endif
}

void TJC_LCDM_Process(void)
{
  static uint32_t s_last_refresh_ms;
#if SERVO_TJC_VISUAL_TEST
  static uint8_t s_dim_high;
  static bool s_visual_started;
#endif
  uint32_t now_ms = HAL_GetTick();

#if SERVO_TJC_VISUAL_TEST
  if (s_visual_started &&
      ((uint32_t)(now_ms - s_last_refresh_ms) < TJC_LCDM_VISUAL_TEST_MS))
  {
    return;
  }
#else
  if (!s_refresh_requested &&
      ((uint32_t)(now_ms - s_last_refresh_ms) < TJC_LCDM_REFRESH_MS))
  {
    return;
  }
#endif

  s_refresh_requested = false;
  s_last_refresh_ms = now_ms;
#if SERVO_TJC_VISUAL_TEST
  s_visual_started = true;
#endif

#if SERVO_TJC_VISUAL_TEST
  if (s_dim_high != 0U)
  {
    send_cmd("dim=20");
    s_dim_high = 0U;
  }
  else
  {
    send_cmd("dim=100");
    s_dim_high = 1U;
  }
  return;
#endif

  if (s_page == 1U)
  {
    send_param_page();
  }
  else
  {
    send_home_page();
  }
}

void TJC_LCDM_ForceRefresh(void)
{
  invalidate_page_cache();
  s_refresh_requested = true;
}

void TJC_LCDM_IRQHandler(void)
{
#if SERVO_TJC_RX_ENABLE
  HAL_UART_IRQHandler(&g_tjc_lcdm_uart);
#endif
}

bool TJC_LCDM_ReadTouchEvent(TJC_LCDMTouchEvent *event)
{
#if SERVO_TJC_RX_ENABLE
  uint32_t primask;
  if (event == NULL)
  {
    return false;
  }
  primask = __get_PRIMASK();
  __disable_irq();
  if (s_touch_seq == s_touch_consumed_seq)
  {
    if (primask == 0U) __enable_irq();
    return false;
  }
  event->x = s_touch_x;
  event->y = s_touch_y;
  event->event = s_touch_event;
  s_touch_consumed_seq = s_touch_seq;
  if (primask == 0U) __enable_irq();
  return true;
#else
  (void)event;
  return false;
#endif
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
#if SERVO_TJC_RX_ENABLE
  if (huart->Instance != USART1)
  {
    return;
  }

  s_tjc_rx_byte_count++;

  if (s_rx_byte == 0xFFU)
  {
    s_rx_ff_count++;
    if (s_rx_ff_count >= 3U)
    {
      process_packet(s_rx_packet, s_rx_len);
      s_rx_len = 0U;
      s_rx_ff_count = 0U;
    }
  }
  else
  {
    s_rx_ff_count = 0U;
    if (s_rx_len < sizeof(s_rx_packet))
    {
      s_rx_packet[s_rx_len++] = s_rx_byte;
    }
    else
    {
      s_rx_len = 0U;
    }
  }

  start_receive();
#else
  (void)huart;
#endif
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
#if SERVO_TJC_RX_ENABLE
  if (huart->Instance == USART1)
  {
    s_tjc_uart_error_count++;
    s_rx_len = 0U;
    s_rx_ff_count = 0U;
    start_receive();
  }
#else
  (void)huart;
#endif
}

#else

void TJC_LCDM_Init(void)
{
}

void TJC_LCDM_Process(void)
{
}

void TJC_LCDM_ForceRefresh(void)
{
}

void TJC_LCDM_IRQHandler(void)
{
}

bool TJC_LCDM_ReadTouchEvent(TJC_LCDMTouchEvent *event)
{
  (void)event;
  return false;
}

#endif
