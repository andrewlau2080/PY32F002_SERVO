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

UART_HandleTypeDef g_tjc_lcdm_uart;

static uint8_t s_rx_byte;
static uint8_t s_rx_packet[64];
static uint8_t s_rx_len;
static uint8_t s_rx_ff_count;
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

#if SERVO_TJC_SOFT_TX
static void soft_tx_cmd_at_baud(uint32_t baud, const char *cmd)
{
  s_soft_tx_baud = baud;
  send_cmd(cmd);
}
#endif

#if defined(SERVO_R01_PRODUCTION_LOOP_TEST) && SERVO_R01_PRODUCTION_LOOP_TEST
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
  static const uint16_t row_y[] = {42U, 80U, 118U, 156U, 194U};
  uint16_t bg = (row & 1U) ? 65535U : 61374U;
  uint16_t y = row_y[row];

  r01_draw_fill(18U, (uint16_t)(y - 2U), 444U, 38U, bg);
  r01_draw_text(26U, y, 230U, 36U, 31U, bg, 0U, name);
  r01_draw_text(286U, y, 152U, 36U, value_color, bg, 0U, value);
}

static void r01_draw_value(uint8_t row, const char *value, uint16_t color)
{
  static const uint16_t row_y[] = {42U, 80U, 118U, 156U, 194U};
  uint16_t bg = (row & 1U) ? 65535U : 61374U;
  r01_draw_text(286U, row_y[row], 152U, 36U, color, bg, 0U, value);
}

void TJC_LCDM_ShowR01Status(uint16_t pulse_us, uint16_t feedback_adc,
                            uint16_t target_adc, int16_t error_adc)
{
  static bool page_drawn;
  static uint16_t last_pulse_us = 0U;
  static uint32_t last_draw_ms;
  char value[24];
  uint32_t now = HAL_GetTick();
  /* UART rendering is synchronous; 100 ms full-page redraws can starve the
     production 1 ms control loop. Refresh on command change, otherwise at 1 Hz. */
  if ((pulse_us == last_pulse_us) && ((uint32_t)(now - last_draw_ms) < 1000U))
  {
    return;
  }
  if (!page_drawn)
  {
    r01_draw_cmd("page 0");
    r01_draw_cmd("ref_stop");
    r01_draw_cmd("cls 65535");
    HAL_Delay(30U);
    r01_draw_fill(0U, 0U, 480U, 32U, 16U);
    r01_draw_text(6U, 3U, 340U, 26U, 65535U, 16U, 0U, "P50 STEP");
    r01_draw_text(350U, 3U, 124U, 26U, 65535U, 16U, 2U, "R01");
    r01_draw_row(0U, "P", "-- us", 31U);
    r01_draw_row(1U, "T", "--", 31U);
    r01_draw_row(2U, "A", "--", 31U);
    r01_draw_row(3U, "E", "--", 63488U);
    r01_draw_row(4U, "R01", "WAIT", 65504U);
    r01_draw_cmd("ref_star");
    page_drawn = true;
  }

  last_pulse_us = pulse_us;
  last_draw_ms = now;
  (void)snprintf(value, sizeof(value), "P%u A%u", (unsigned)pulse_us,
                 (unsigned)feedback_adc);
  r01_draw_value(4U, value, (error_adc < 0) ? 65504U : 31U);
  (void)target_adc;
}
#endif

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
  char cmd[24];
  static const uint32_t recovery_baud[] = {9600U, 38400U, 115200U};
  soft_tx_init();
  /* Match the known-good V9.x PA7 display recovery sequence. */
  for (uint32_t i = 0U; i < (sizeof(recovery_baud) / sizeof(recovery_baud[0])); i++)
  {
    soft_tx_cmd_at_baud(recovery_baud[i], "bkcmd=0");
    (void)snprintf(cmd, sizeof(cmd), "bauds=%u", 9600U);
    soft_tx_cmd_at_baud(recovery_baud[i], cmd);
    soft_tx_cmd_at_baud(recovery_baud[i], "baud=9600");
    HAL_Delay(50U);
  }
  soft_tx_cmd_at_baud(9600U, "bkcmd=0");
  HAL_Delay(80U);
  soft_tx_cmd_at_baud(9600U, "dim=100");
  soft_tx_cmd_at_baud(9600U, "page 0");
  HAL_Delay(250U);
  soft_tx_cmd_at_baud(9600U, "sendxy=1");
  s_soft_tx_baud = 9600U;
  s_page = 0U;
  s_tjc_debug_page = s_page;
  invalidate_page_cache();
  s_refresh_requested = true;
#else
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

#endif
