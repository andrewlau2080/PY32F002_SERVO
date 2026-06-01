#include "tjc_lcdm.h"

#if SERVO_ENABLE_TJC_LCDM

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "servo_comm.h"
#include "servo_params.h"
#include "py32f0xx_hal_uart.h"

UART_HandleTypeDef g_tjc_lcdm_uart;

static uint8_t s_rx_byte;
static uint8_t s_rx_packet[64];
static uint8_t s_rx_len;
static uint8_t s_rx_ff_count;
static uint8_t s_page;
static bool s_refresh_requested;

static void send_end(void)
{
  static const uint8_t end_bytes[3] = {0xFFU, 0xFFU, 0xFFU};
  (void)HAL_UART_Transmit(&g_tjc_lcdm_uart, (uint8_t *)end_bytes, sizeof(end_bytes), 10U);
}

static void send_cmd(const char *cmd)
{
  (void)HAL_UART_Transmit(&g_tjc_lcdm_uart, (uint8_t *)cmd, (uint16_t)strlen(cmd), 20U);
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

  set_num("n0", telemetry.input_us);
  set_num("n1", telemetry.target_adc);
  set_num("n2", telemetry.feedback_adc);
  set_num("n3", telemetry.error_adc);
  set_num("n4", telemetry.motor_duty);
  set_num("n5", telemetry.vdd_mv);
  set_num("j0", target_percent);
  set_num("j1", feedback_percent);
  set_num("j2", duty_percent);
  set_text("t0", state_text(telemetry.servo_state));
  set_text("t1", (telemetry.params_valid != 0U) ? "CRC OK" : "CRC BAD");
}

static void send_param_page(void)
{
  const ServoParams *params = Servo_Params_Get();

  set_num("n10", params->pulse_lower_us);
  set_num("n11", params->pulse_center_us);
  set_num("n12", params->pulse_high_us);
  set_num("n13", params->adc_min_count);
  set_num("n14", params->adc_center_count);
  set_num("n15", params->adc_max_count);
  set_num("n16", params->deadband_us);
  set_num("n17", params->deadband_count_min);
  set_num("n18", params->max_duty);
  set_num("n19", params->boost_duty);
}

static void process_packet(const uint8_t *packet, uint8_t len)
{
  if (len == 0U)
  {
    return;
  }

  if ((len >= 3U) && (packet[0] == 0x65U))
  {
    s_page = packet[1];
    s_refresh_requested = true;
    return;
  }

  if ((len >= 6U) && (memcmp(packet, "page=", 5U) == 0))
  {
    s_page = (uint8_t)(packet[5] - (uint8_t)'0');
    s_refresh_requested = true;
    return;
  }

  if ((len >= 7U) && (memcmp(packet, "refresh", 7U) == 0))
  {
    s_refresh_requested = true;
  }
}

static void start_receive(void)
{
  (void)HAL_UART_Receive_IT(&g_tjc_lcdm_uart, &s_rx_byte, 1U);
}

void TJC_LCDM_Init(void)
{
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
  s_refresh_requested = true;
  start_receive();
}

void TJC_LCDM_Process(void)
{
  static uint32_t s_last_refresh_ms;
  uint32_t now_ms = HAL_GetTick();

  if (!s_refresh_requested &&
      ((uint32_t)(now_ms - s_last_refresh_ms) < TJC_LCDM_REFRESH_MS))
  {
    return;
  }

  s_refresh_requested = false;
  s_last_refresh_ms = now_ms;

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
  s_refresh_requested = true;
}

void TJC_LCDM_IRQHandler(void)
{
  HAL_UART_IRQHandler(&g_tjc_lcdm_uart);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART1)
  {
    return;
  }

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
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    s_rx_len = 0U;
    s_rx_ff_count = 0U;
    start_receive();
  }
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
