#include "main.h"
#include "adc_feedback.h"
#include "board.h"
#include "hbridge.h"
#include "pwm_input.h"
#include "servo_config.h"
#include "servo_control.h"
#include "servo_params.h"
#include "tjc_lcdm.h"
#if SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
#include "py32f0xx_hal_uart.h"
#endif
#include <stdio.h>
#include <string.h>

#ifndef SERVO_ENABLE_HBRIDGE_IO_TEST
#define SERVO_ENABLE_HBRIDGE_IO_TEST 0
#endif

#ifndef SERVO_ENABLE_ADC_LED_TEST
#define SERVO_ENABLE_ADC_LED_TEST 0
#endif

#ifndef SERVO_ENABLE_PA7_SQUARE_TEST
#define SERVO_ENABLE_PA7_SQUARE_TEST 0
#endif

#ifndef SERVO_ENABLE_PA7_TJC_TEST
#define SERVO_ENABLE_PA7_TJC_TEST 0
#endif

#ifndef SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
#define SERVO_ENABLE_LCDM_RUNTIME_UI_TEST 0
#endif

#ifndef SERVO_ENABLE_INTERNAL_STEP_TEST
#define SERVO_ENABLE_INTERNAL_STEP_TEST 0
#endif

#ifndef SERVO_TJC_SOFT_TX
#define SERVO_TJC_SOFT_TX 0
#endif

#if SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
static int16_t lcdm_servo_loop_update_fast(void);
static void lcdm_runtime_service_motor_control(void);
#endif

static void app_use_flash_vector_table(void)
{
  SCB->VTOR = FLASH_BASE;
  __DSB();
  __ISB();
  __enable_irq();
}

void _init(void)
{
}

void _fini(void)
{
}

#if SERVO_ENABLE_PA7_SQUARE_TEST || SERVO_ENABLE_PA7_TJC_TEST || SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
static void wait_systick_ticks(uint32_t ticks)
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
#endif

#if SERVO_ENABLE_PA7_SQUARE_TEST
static void run_pa7_square_test(void)
{
  GPIO_InitTypeDef gpio = {0};
  uint32_t half_cycles = 0U;
  uint32_t half_period_ticks;
  bool state = false;

  Board_Init();

  __HAL_RCC_GPIOA_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_7;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &gpio);
  GPIOA->BRR = GPIO_PIN_7;

  half_period_ticks = SystemCoreClock / 2000U;
  __disable_irq();
  while (1)
  {
    wait_systick_ticks(half_period_ticks);
    if (state)
    {
      GPIOA->BRR = GPIO_PIN_7;
      state = false;
    }
    else
    {
      GPIOA->BSRR = GPIO_PIN_7;
      state = true;
    }

    half_cycles++;
    if ((half_cycles % 1000U) < 80U)
    {
      GPIOA->BRR = GPIO_PIN_5;
    }
    else
    {
      GPIOA->BSRR = GPIO_PIN_5;
    }
  }
}
#endif

#if SERVO_ENABLE_PA7_TJC_TEST || SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
static volatile uint32_t s_pa7_tjc_rx_byte_count;
static volatile uint32_t s_pa7_tjc_rx_frame_error_count;
static volatile uint8_t s_pa7_tjc_last_rx_byte;
static uint32_t s_pa7_tjc_baud = 9600U;
#if SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
static UART_HandleTypeDef s_lcdm_runtime_uart;
static bool s_lcdm_runtime_uart_ready;
static bool s_lcdm_runtime_control_ready;
static uint32_t s_lcdm_runtime_last_motor_ms;
static volatile uint8_t s_lcdm_rx_ring[64];
static volatile uint8_t s_lcdm_rx_head;
static volatile uint8_t s_lcdm_rx_tail;
static volatile uint16_t s_lcdm_rx_overflow_count;
static uint8_t s_lcdm_touch_frame[9];
static uint8_t s_lcdm_touch_index;
#endif

#ifndef LCDM_TJC_BAUD
#define LCDM_TJC_BAUD           9600U
#endif
#ifndef LCDM_TJC_RECOVERY_BAUD
#define LCDM_TJC_RECOVERY_BAUD  38400U
#endif
#ifndef LCDM_TJC_RECOVERY_BAUD2
#define LCDM_TJC_RECOVERY_BAUD2 115200U
#endif
#ifndef LCDM_TJC_CMD_GAP_MS
#define LCDM_TJC_CMD_GAP_MS     2U
#endif
#ifndef LCDM_TJC_TX_TIMEOUT_MS
#define LCDM_TJC_TX_TIMEOUT_MS  250U
#endif
#ifndef LCDM_TJC_PAGE_CLEAR_MS
#define LCDM_TJC_PAGE_CLEAR_MS  30U
#endif
#ifndef LCDM_TJC_FONT_ID
#define LCDM_TJC_FONT_ID        0U
#endif

static uint32_t app_irq_save(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

static void app_irq_restore(uint32_t primask)
{
  __set_PRIMASK(primask);
}

#if SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
static void lcdm_runtime_service_motor_control(void)
{
  uint32_t now_ms;
  uint8_t loops = 0U;

  if (!s_lcdm_runtime_control_ready)
  {
    return;
  }

  now_ms = HAL_GetTick();
  while ((uint32_t)(now_ms - s_lcdm_runtime_last_motor_ms) >= 1U)
  {
    (void)lcdm_servo_loop_update_fast();
    s_lcdm_runtime_last_motor_ms++;
    loops++;
    if (loops >= 4U)
    {
      s_lcdm_runtime_last_motor_ms = HAL_GetTick();
      break;
    }
    now_ms = HAL_GetTick();
  }
}

static void lcdm_runtime_wait_ms_with_control(uint32_t ms)
{
  for (uint32_t i = 0U; i < ms; i++)
  {
    lcdm_runtime_service_motor_control();
    wait_systick_ticks(SystemCoreClock / 1000U);
  }
  lcdm_runtime_service_motor_control();
}

static void lcdm_rx_ring_reset(void)
{
  uint32_t primask = app_irq_save();
  s_lcdm_rx_head = 0U;
  s_lcdm_rx_tail = 0U;
  s_lcdm_touch_index = 0U;
  app_irq_restore(primask);
}

static void lcdm_rx_ring_push(uint8_t byte)
{
  uint8_t next = (uint8_t)((s_lcdm_rx_head + 1U) & 0x3FU);
  if (next == s_lcdm_rx_tail)
  {
    s_lcdm_rx_overflow_count++;
    return;
  }
  s_lcdm_rx_ring[s_lcdm_rx_head] = byte;
  s_lcdm_rx_head = next;
}

static bool lcdm_rx_ring_pop(uint8_t *byte)
{
  uint32_t primask;

  if (s_lcdm_rx_tail == s_lcdm_rx_head)
  {
    return false;
  }

  primask = app_irq_save();
  if (s_lcdm_rx_tail == s_lcdm_rx_head)
  {
    app_irq_restore(primask);
    return false;
  }
  *byte = s_lcdm_rx_ring[s_lcdm_rx_tail];
  s_lcdm_rx_tail = (uint8_t)((s_lcdm_rx_tail + 1U) & 0x3FU);
  app_irq_restore(primask);
  return true;
}

static void lcdm_runtime_uart_set_baud(uint32_t baud)
{
  if (s_lcdm_runtime_uart_ready)
  {
    (void)HAL_UART_DeInit(&s_lcdm_runtime_uart);
  }

  s_lcdm_runtime_uart.Instance = USART1;
  s_lcdm_runtime_uart.Init.BaudRate = baud;
  s_lcdm_runtime_uart.Init.WordLength = UART_WORDLENGTH_8B;
  s_lcdm_runtime_uart.Init.StopBits = UART_STOPBITS_1;
  s_lcdm_runtime_uart.Init.Parity = UART_PARITY_NONE;
  s_lcdm_runtime_uart.Init.Mode = UART_MODE_TX_RX;
  s_lcdm_runtime_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  s_lcdm_runtime_uart.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&s_lcdm_runtime_uart) != HAL_OK)
  {
    APP_ErrorHandler();
  }
  s_lcdm_runtime_uart_ready = true;
  s_pa7_tjc_baud = baud;
  lcdm_rx_ring_reset();
  USART1->CR1 |= USART_CR1_RXNEIE;
  USART1->CR3 |= USART_CR3_EIE;
  HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

void LCDM_Runtime_IRQHandler(void)
{
  uint32_t sr = USART1->SR;

  if ((sr & (USART_SR_RXNE | USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE)) != 0U)
  {
    uint8_t byte = (uint8_t)(USART1->DR & 0xFFU);
    if ((sr & USART_SR_RXNE) != 0U)
    {
      lcdm_rx_ring_push(byte);
      s_pa7_tjc_last_rx_byte = byte;
      s_pa7_tjc_rx_byte_count++;
    }
    if ((sr & (USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE)) != 0U)
    {
      s_pa7_tjc_rx_frame_error_count++;
    }
  }
}
#endif

#if (!SERVO_ENABLE_LCDM_RUNTIME_UI_TEST) || SERVO_TJC_SOFT_TX
static bool pb2_tjc_rx_is_high(void)
{
  return ((GPIOB->IDR & GPIO_PIN_2) != 0U);
}

static uint32_t systick_elapsed_since(uint32_t *previous)
{
  uint32_t elapsed;
  uint32_t current = SysTick->VAL;
  uint32_t reload = SysTick->LOAD + 1U;

  if (*previous >= current)
  {
    elapsed = *previous - current;
  }
  else
  {
    elapsed = *previous + (reload - current);
  }

  *previous = current;
  return elapsed;
}
#endif

static void pa7_tjc_delay_ms_with_led(uint32_t ms)
{
  for (uint32_t i = 0; i < ms; i++)
  {
    uint32_t phase = (s_pa7_tjc_rx_byte_count != 0U) ? (i % 120U) : (i % 500U);
    uint32_t on_ms = (s_pa7_tjc_rx_byte_count != 0U) ? 20U : 40U;

    if (phase < on_ms)
    {
      GPIOA->BRR = GPIO_PIN_5;
    }
    else
    {
      GPIOA->BSRR = GPIO_PIN_5;
    }
#if SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
    lcdm_runtime_service_motor_control();
#endif
    wait_systick_ticks(SystemCoreClock / 1000U);
  }
#if SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
  lcdm_runtime_service_motor_control();
#endif
}

static void pb2_tjc_clear_rx_errors(void)
{
#if SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
  volatile uint32_t sr;
  volatile uint32_t dr;

  if (!s_lcdm_runtime_uart_ready)
  {
    return;
  }

  sr = USART1->SR;
  if ((sr & (USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE)) != 0U)
  {
    dr = USART1->DR;
    (void)sr;
    (void)dr;
    s_lcdm_runtime_uart.ErrorCode = HAL_UART_ERROR_NONE;
    s_pa7_tjc_rx_frame_error_count++;
  }
#endif
}

static void pb2_tjc_drain_rx(void)
{
#if SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
  if (!s_lcdm_runtime_uart_ready)
  {
    return;
  }

  pb2_tjc_clear_rx_errors();
  while ((USART1->SR & USART_SR_RXNE) != 0U)
  {
    uint8_t byte = (uint8_t)(USART1->DR & 0xFFU);
    lcdm_rx_ring_push(byte);
    pb2_tjc_clear_rx_errors();
  }
#endif
}

static bool pb2_tjc_read_byte(uint8_t *value, uint32_t timeout_ms)
{
#if SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
  uint32_t start = HAL_GetTick();

  do
  {
    pb2_tjc_clear_rx_errors();
    pb2_tjc_drain_rx();
    if (lcdm_rx_ring_pop(value))
    {
      return true;
    }
  } while ((uint32_t)(HAL_GetTick() - start) < timeout_ms);

  return false;
#else
  uint32_t bit_ticks = SystemCoreClock / s_pa7_tjc_baud;
  uint32_t timeout_ticks = (SystemCoreClock / 1000U) * timeout_ms;
  uint32_t elapsed = 0U;
  uint32_t previous = SysTick->VAL;
  uint8_t byte = 0U;
  bool was_high = pb2_tjc_rx_is_high();

  while (elapsed < timeout_ticks)
  {
    bool is_high = pb2_tjc_rx_is_high();
    if (was_high && !is_high)
    {
      uint32_t primask = app_irq_save();
      wait_systick_ticks(bit_ticks + (bit_ticks / 2U));
      for (uint32_t bit = 0U; bit < 8U; bit++)
      {
        if (pb2_tjc_rx_is_high())
        {
          byte |= (uint8_t)(1U << bit);
        }
        wait_systick_ticks(bit_ticks);
      }

      if (!pb2_tjc_rx_is_high())
      {
        app_irq_restore(primask);
        s_pa7_tjc_rx_frame_error_count++;
        return false;
      }

      app_irq_restore(primask);
      *value = byte;
      s_pa7_tjc_last_rx_byte = byte;
      s_pa7_tjc_rx_byte_count++;
      return true;
    }

    was_high = is_high;
    elapsed += systick_elapsed_since(&previous);
  }

  return false;
#endif
}

#if SERVO_ENABLE_PA7_TJC_TEST
static void pb2_tjc_poll_rx_ms(uint32_t ms)
{
  uint32_t start_count = s_pa7_tjc_rx_byte_count;
  for (uint32_t i = 0; i < ms; i++)
  {
    uint8_t byte;
    while (pb2_tjc_read_byte(&byte, 1U))
    {
    }
    if (s_pa7_tjc_rx_byte_count != start_count)
    {
      GPIOA->BRR = GPIO_PIN_5;
    }
  }
}
#endif

#if (!SERVO_ENABLE_LCDM_RUNTIME_UI_TEST) || SERVO_TJC_SOFT_TX
static void pa7_tjc_soft_tx_init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIOA->BSRR = GPIO_PIN_7;
  gpio.Pin = GPIO_PIN_7;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &gpio);
}

static void pa7_tjc_write_byte(uint8_t value)
{
  uint32_t bit_ticks = SystemCoreClock / s_pa7_tjc_baud;

  GPIOA->BRR = GPIO_PIN_7;
  wait_systick_ticks(bit_ticks);

  for (uint32_t bit = 0U; bit < 8U; bit++)
  {
    if ((value & (1U << bit)) != 0U)
    {
      GPIOA->BSRR = GPIO_PIN_7;
    }
    else
    {
      GPIOA->BRR = GPIO_PIN_7;
    }
    wait_systick_ticks(bit_ticks);
  }

  GPIOA->BSRR = GPIO_PIN_7;
  wait_systick_ticks(bit_ticks);
}
#endif

static void pa7_tjc_write_cmd(const char *cmd)
{
#if SERVO_ENABLE_LCDM_RUNTIME_UI_TEST && !SERVO_TJC_SOFT_TX
  static const uint8_t end_bytes[3] = {0xFFU, 0xFFU, 0xFFU};
  const char *cursor = cmd;
  size_t remaining = strlen(cmd);
  pb2_tjc_drain_rx();
  while (remaining > 0U)
  {
    uint16_t chunk = (remaining > 16U) ? 16U : (uint16_t)remaining;
    (void)HAL_UART_Transmit(&s_lcdm_runtime_uart, (uint8_t *)cursor, chunk, LCDM_TJC_TX_TIMEOUT_MS);
    cursor += chunk;
    remaining -= chunk;
    lcdm_runtime_service_motor_control();
  }
  (void)HAL_UART_Transmit(&s_lcdm_runtime_uart, (uint8_t *)end_bytes, sizeof(end_bytes), 20U);
  lcdm_runtime_wait_ms_with_control(LCDM_TJC_CMD_GAP_MS);
  pb2_tjc_drain_rx();
#else
  while (*cmd != '\0')
  {
    pa7_tjc_write_byte((uint8_t)*cmd);
    cmd++;
  }

  pa7_tjc_write_byte(0xFFU);
  pa7_tjc_write_byte(0xFFU);
  pa7_tjc_write_byte(0xFFU);
#endif
}

static void pa7_tjc_send_baud_cmds(uint32_t source_baud, uint32_t target_baud)
{
  char cmd[24];

  s_pa7_tjc_baud = source_baud;
#if SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
  lcdm_runtime_uart_set_baud(source_baud);
#endif
  pa7_tjc_write_cmd("bkcmd=0");
  (void)snprintf(cmd, sizeof(cmd), "bauds=%lu", (unsigned long)target_baud);
  pa7_tjc_write_cmd(cmd);
  (void)snprintf(cmd, sizeof(cmd), "baud=%lu", (unsigned long)target_baud);
  pa7_tjc_write_cmd(cmd);
  pa7_tjc_delay_ms_with_led(50U);
}

static void pa7_tjc_try_recovery_baud(uint32_t source_baud, uint32_t target_baud)
{
  if ((source_baud == 0U) ||
      ((source_baud == target_baud) && (target_baud != LCDM_TJC_BAUD)) ||
      (source_baud == LCDM_TJC_BAUD))
  {
    return;
  }
  pa7_tjc_send_baud_cmds(source_baud, target_baud);
}

static void pa7_tjc_force_default_baud(void)
{
  pa7_tjc_try_recovery_baud(9600U, LCDM_TJC_BAUD);
  pa7_tjc_try_recovery_baud(LCDM_TJC_RECOVERY_BAUD, LCDM_TJC_BAUD);
  pa7_tjc_try_recovery_baud(LCDM_TJC_RECOVERY_BAUD2, LCDM_TJC_BAUD);
  pa7_tjc_send_baud_cmds(LCDM_TJC_BAUD, LCDM_TJC_BAUD);

  s_pa7_tjc_baud = LCDM_TJC_BAUD;
#if SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
  lcdm_runtime_uart_set_baud(LCDM_TJC_BAUD);
#endif
  pa7_tjc_write_cmd("bkcmd=0");
  pa7_tjc_delay_ms_with_led(80U);
}

static void pa7_tjc_disable_hmi_touch_objects(void)
{
  char cmd[16];

  for (uint8_t retry = 0U; retry < 3U; retry++)
  {
    pa7_tjc_write_cmd("tsw 255,0");
  }

  for (uint8_t id = 0U; id < 64U; id++)
  {
    (void)snprintf(cmd, sizeof(cmd), "tsw %u,0", (unsigned)id);
    pa7_tjc_write_cmd(cmd);
  }
}

static void pa7_tjc_runtime_init_display(void)
{
  pa7_tjc_write_cmd("bkcmd=0");
  pa7_tjc_write_cmd("dim=100");
  pa7_tjc_write_cmd("page 0");
  pa7_tjc_delay_ms_with_led(250U);
  pa7_tjc_disable_hmi_touch_objects();
  pa7_tjc_write_cmd("sendxy=1");
}

#if SERVO_ENABLE_PA7_TJC_TEST
static void pa7_tjc_set_num(const char *obj, int32_t value)
{
  char cmd[32];
  (void)snprintf(cmd, sizeof(cmd), "%s.val=%ld", obj, (long)value);
  pa7_tjc_write_cmd(cmd);
}

static void pa7_tjc_set_text(const char *obj, const char *value)
{
  char cmd[48];
  (void)snprintf(cmd, sizeof(cmd), "%s.txt=\"%s\"", obj, value);
  pa7_tjc_write_cmd(cmd);
}

static void pa7_tjc_send_home_layout(uint16_t step)
{
  uint16_t target = (uint16_t)((step * 7U) % 101U);
  uint16_t feedback = (uint16_t)((target + 93U) % 101U);
  int16_t error = (int16_t)target - (int16_t)feedback;
  int16_t duty = (int16_t)((error >= 0) ? (error * 8) : (error * 8));

  if (duty > 850)
  {
    duty = 850;
  }
  else if (duty < -850)
  {
    duty = -850;
  }

  pa7_tjc_set_num("n0", 900 + ((step * 13U) % 1201U));
  pa7_tjc_set_num("n1", (uint16_t)(600U + ((uint32_t)target * 2895U) / 100U));
  pa7_tjc_set_num("n2", (uint16_t)(600U + ((uint32_t)feedback * 2895U) / 100U));
  pa7_tjc_set_num("n3", error);
  pa7_tjc_set_num("n4", duty);
  pa7_tjc_set_num("n5", 3300);
  pa7_tjc_set_num("j0", target);
  pa7_tjc_set_num("j1", feedback);
  pa7_tjc_set_num("j2", (duty >= 0) ? duty / 10 : -duty / 10);
  pa7_tjc_set_text("t0", (step & 1U) ? "DRV" : "HLD");
  pa7_tjc_set_text("t1", (s_pa7_tjc_rx_byte_count != 0U) ? "RX" : "WT");
}

static void pa7_tjc_send_param_layout(uint16_t step)
{
  pa7_tjc_set_num("n10", 900);
  pa7_tjc_set_num("n11", 1500);
  pa7_tjc_set_num("n12", 2100);
  pa7_tjc_set_num("n13", 600);
  pa7_tjc_set_num("n14", 2048);
  pa7_tjc_set_num("n15", 3495);
  pa7_tjc_set_num("n16", 2 + (step % 4U));
  pa7_tjc_set_num("n17", 8);
  pa7_tjc_set_num("n18", 850);
  pa7_tjc_set_num("n19", 160);
}

static void run_pa7_tjc_test(void)
{
  GPIO_InitTypeDef gpio = {0};
  uint16_t step = 0U;

  Board_Init();

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  pa7_tjc_soft_tx_init();

  gpio.Pin = GPIO_PIN_2;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);

  __disable_irq();
  while (1)
  {
    pa7_tjc_write_cmd("bkcmd=3");
    pa7_tjc_write_cmd("dim=100");

    pa7_tjc_write_cmd("page 0");
    pb2_tjc_poll_rx_ms(30U);
    for (uint32_t i = 0; i < 12U; i++)
    {
      pa7_tjc_send_home_layout(step++);
      pb2_tjc_poll_rx_ms(30U);
      pa7_tjc_delay_ms_with_led(120U);
    }

    pa7_tjc_write_cmd("page 1");
    pb2_tjc_poll_rx_ms(30U);
    for (uint32_t i = 0; i < 6U; i++)
    {
      pa7_tjc_send_param_layout(step++);
      pb2_tjc_poll_rx_ms(30U);
      pa7_tjc_delay_ms_with_led(200U);
    }
  }
}
#endif

#if SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
#define LCDM_COLOR_BLACK        0U
#define LCDM_COLOR_BLUE         31U
#define LCDM_COLOR_GREEN        2016U
#define LCDM_COLOR_CYAN         2047U
#define LCDM_COLOR_RED          63488U
#define LCDM_COLOR_YELLOW       65504U
#define LCDM_COLOR_WHITE        65535U
#define LCDM_COLOR_GRAY         33808U
#define LCDM_COLOR_DARK_GRAY    16904U
#define LCDM_COLOR_NAVY         16U
#define LCDM_COLOR_SOFT_BLUE    1055U
#define LCDM_COLOR_ROW_ALT      61374U

#define LCDM_ARRAY_COUNT(a)     (sizeof(a) / sizeof((a)[0]))
#define LCDM_W                  480U
#define LCDM_H                  272U
#define LCDM_TITLE_H            32U
#define LCDM_FOOTER_Y           240U
#define LCDM_FOOTER_H           32U
#define LCDM_FOOTER_SEG_W       (LCDM_W / 3U)
#define LCDM_ROW_BG_X           18U
#define LCDM_ROW_X              26U
#define LCDM_ROW_W              444U
#define LCDM_ROW_NAME_W         230U
#define LCDM_ROW_VALUE_X        286U
#define LCDM_ROW_VALUE_W        152U
#define LCDM_ROW_Y              42U
#define LCDM_ROW_H              36U
#define LCDM_ROW_STEP           38U
#define LCDM_VISIBLE_ROWS       5U
#define LCDM_MENU_GRID_X        26U
#define LCDM_MENU_GRID_Y        40U
#define LCDM_MENU_CELL_W        196U
#define LCDM_MENU_CELL_H        36U
#define LCDM_MENU_COL_STEP      232U
#define LCDM_MENU_ROW_STEP      39U
#define LCDM_MENU_ROWS          5U
#define LCDM_MENU_COLS          2U
#define LCDM_MENU_VISIBLE_COUNT (LCDM_MENU_ROWS * LCDM_MENU_COLS)
#define LCDM_MENU_ROW_Y         LCDM_MENU_GRID_Y
#define LCDM_MENU_ROW_H         LCDM_MENU_CELL_H
#define LCDM_MENU_ROW_NONE      0xFFU
#define LCDM_TOUCH_Y_SLOP       8U
#define LCDM_FOOTER_TOUCH_Y     232U
#define LCDM_ADC_TEST_MIN_RAW   213U
#define LCDM_ADC_TEST_MAX_RAW   3900U
#define LCDM_ADC_TEST_OFF_BAND  12U
#define LCDM_ADC_TEST_CENTER_RAW ((LCDM_ADC_TEST_MIN_RAW + LCDM_ADC_TEST_MAX_RAW) / 2U)
#define LCDM_SERVO_INPUT_MID_US 1500U
#define LCDM_SERVO_WORK_LOW_MV  250U
#define LCDM_SERVO_WORK_MID_MV  1650U
#define LCDM_SERVO_WORK_HIGH_MV 3050U
#define LCDM_SERVO_STATE_NO_PWM 0U
#define LCDM_SERVO_STATE_HOLD   1U
#define LCDM_SERVO_STATE_DRIVE  2U
#define LCDM_SERVO_STATE_BAD    3U
#define LCDM_SERVO_STATE_DIR_ALARM 4U
#define LCDM_SERVO_STATE_LOW_LIMIT 5U
#define LCDM_SERVO_STATE_HIGH_LIMIT 6U
#define LCDM_SERVO_STATE_RANGE_OFF 7U
#define LCDM_SERVO_STATE_ADC_LOST 8U
#define LCDM_SERVO_STATE_BRAKE  9U
#define LCDM_SERVO_RANGE_RELEASE   0U
#define LCDM_SERVO_RANGE_LOCK      1U
#define LCDM_STARTUP_PARAM_RANGE_MODE 0U
#define LCDM_STARTUP_PARAM_INPUT_STABLE_BAND 3U
#define LCDM_STARTUP_PARAM_CMD_DEADBAND 4U
#define LCDM_PWM_SOURCE_EXTERNAL 0U
#define LCDM_PWM_SOURCE_INTERNAL 1U
#define LCDM_SERVO_DIR_CHECK_MIN_DUTY 180
#define LCDM_SERVO_DIR_CHECK_MIN_ERROR 60U
#define LCDM_SERVO_DIR_CHECK_MIN_STEP 1U
#define LCDM_SERVO_DIR_CHECK_MAX_STEP 8U
#define LCDM_SERVO_DIR_CHECK_TRIP_COUNT 80U
#define LCDM_SERVO_ADC_NOISE_BAND 3U
#define LCDM_SERVO_ADC_DISPLAY_QUIET_BAND 12U
#define LCDM_SERVO_ADC_JUMP_BAND 36U
#define LCDM_SERVO_ADC_JUMP_CONFIRM 3U
#define LCDM_SERVO_CMD_HOLD_EXIT_EXTRA 3U
#define LCDM_SERVO_CMD_MOTION_EXIT_EXTRA 12U
#define LCDM_SERVO_PWM_CMD_DIRECT_US 8U
#define LCDM_SERVO_PWM_CMD_FINE_US 3U
#define LCDM_SERVO_PWM_CMD_FINE_CLUSTER_US 2U
#define LCDM_SERVO_PWM_CMD_REPEAT_CLUSTER_US 1U
#define LCDM_SERVO_PWM_CMD_FINE_COUNT 2U
#define LCDM_SERVO_PWM_CMD_REPEAT_COUNT 6U
#define LCDM_SERVO_FORCE_LOCK_MODE 1U
#define LCDM_SERVO_LOCK_MIN_DUTY 360U
#define LCDM_SERVO_CMD_MIN_DUTY 90U
#define LCDM_SERVO_CMD_REVERSE_LIMIT 75
#define LCDM_SERVO_CMD_DECEL_MIN 120U
#define LCDM_SERVO_CMD_DECEL_DIV 5U
#define LCDM_SERVO_CMD_DECEL_MAX 360U
#define LCDM_SERVO_CMD_FINE_BAND 32U
#define LCDM_SERVO_CMD_TAIL_STEP 20U
#define LCDM_SERVO_CMD_CRUISE_VEL 10U
#define LCDM_SERVO_CMD_BRAKE_GAIN 7U
#define LCDM_SERVO_FAST_FULL_ERROR 820U
#define LCDM_SERVO_CMD_ARRIVE_BAND 18U
#define LCDM_SERVO_CMD_SETTLE_BAND 96U
#define LCDM_SERVO_CMD_SETTLE_COUNT 18U
#define LCDM_SERVO_TRAJ_MAX_VEL_Q4 416
#define LCDM_SERVO_TRAJ_ACCEL_Q4 36
#define LCDM_SERVO_TRAJ_TARGET_TOL_Q4 48
#define LCDM_SERVO_POS_K_SHIFT 2U
#define LCDM_SERVO_VEL_K_NUM 3
#define LCDM_SERVO_OBS_POS_DIV_SLOW 3
#define LCDM_SERVO_OBS_VEL_DIV_SLOW 12
#define LCDM_SERVO_OBS_POS_DIV_FAST 2
#define LCDM_SERVO_OBS_VEL_DIV_FAST 7
#define LCDM_SERVO_OBS_FAST_RESIDUAL_Q4 128
#define LCDM_SERVO_HOLD_BAND 18U
#define LCDM_SERVO_HOLD_VEL_Q4 36
#define LCDM_SERVO_HOLD_EXIT_BAND 34U
#define LCDM_SERVO_HOLD_EXIT_COUNT 1U
#define LCDM_SERVO_TERMINAL_BAND 120U
#define LCDM_SERVO_TERMINAL_FINE_BAND 24U
#define LCDM_SERVO_TERMINAL_VREF_Q4 72
#define LCDM_SERVO_TERMINAL_FINE_VREF_Q4 24
#define LCDM_SERVO_LONG_MOVE_ERROR 1200U
#define LCDM_SERVO_MID_MOVE_ERROR 700U
#define LCDM_SERVO_LONG_TERMINAL_BAND 180U
#define LCDM_SERVO_LONG_TERMINAL_FINE_BAND 32U
#define LCDM_SERVO_LONG_TERMINAL_VREF_Q4 56
#define LCDM_SERVO_LONG_TERMINAL_FINE_VREF_Q4 18
#define LCDM_SERVO_MID_TERMINAL_BAND 150U
#define LCDM_SERVO_MID_TERMINAL_FINE_BAND 28U
#define LCDM_SERVO_MID_TERMINAL_VREF_Q4 64
#define LCDM_SERVO_MID_TERMINAL_FINE_VREF_Q4 20
#define LCDM_SERVO_APPROACH_BRAKE_VEL_Q4 40
#define LCDM_SERVO_APPROACH_BRAKE_GAIN 4U
#define LCDM_SERVO_APPROACH_BRAKE_MAX_BAND 520U
#define LCDM_SERVO_APPROACH_BRAKE_TAIL_COUNT 12U
#define LCDM_SERVO_APPROACH_BRAKE_TAIL_BAND 72U
#define LCDM_SERVO_APPROACH_BRAKE_TAIL_VEL_Q4 8
#define LCDM_SERVO_APPROACH_BRAKE_TAIL_DUTY 80
#define LCDM_SERVO_OVERSHOOT_BRAKE_BAND 180U
#define LCDM_SERVO_OVERSHOOT_BRAKE_VEL_Q4 16
#define LCDM_SERVO_OVERSHOOT_RETURN_DUTY 360
#define LCDM_SERVO_OVERSHOOT_RETURN_NEAR_BAND 48U
#define LCDM_SERVO_OVERSHOOT_RETURN_NEAR_DUTY 180
#define LCDM_SERVO_OVERSHOOT_QUIET_BAND 24U
#define LCDM_SERVO_OVERSHOOT_QUIET_VEL_Q4 12
#define LCDM_SERVO_OVERSHOOT_STUCK_BAND 24U
#define LCDM_SERVO_OVERSHOOT_STUCK_VEL_Q4 16
#define LCDM_SERVO_OVERSHOOT_STUCK_COUNT 18U
#define LCDM_SERVO_OVERSHOOT_STUCK_IMPROVE 3U
#define LCDM_SERVO_TERMINAL_SETTLE_BAND 24U
#define LCDM_SERVO_TERMINAL_SETTLE_DUTY 80
#define LCDM_SERVO_TERMINAL_LOW_DUTY_BRAKE_BAND 160U
#define LCDM_SERVO_TERMINAL_LOW_DUTY_BRAKE 90
#define LCDM_SERVO_OUT_SLEW 80
#define LCDM_SERVO_MOVING_FF_DUTY 120
#define LCDM_SERVO_REVERSE_DEAD_MS 8U
#define LCDM_SERVO_REVERSE_MIN_DUTY 180
#define LCDM_SERVO_TRACK_ERR_LIMIT_Q4 640
#define LCDM_SERVO_HOLD_QUIET_EXTRA 12U
#define LCDM_SERVO_HOLD_CONFIRM_NEAR 2U
#define LCDM_SERVO_HOLD_PULSE_GAIN 4U
#define LCDM_SERVO_HOLD_BRAKE_EXTRA 24U
#define LCDM_SERVO_HOLD_DRIVE_MIN_DUTY 420U
#define LCDM_SERVO_HOLD_ANCHOR_BAND 12U
#define LCDM_SERVO_FIXED_HOLD_BAND 18U
#define LCDM_SERVO_HOLD_RELEASE_BAND 16U
#define LCDM_SERVO_HOLD_QUIET_DRIFT_BAND 18U
#define LCDM_SERVO_HOLD_QUIET_DRIFT_VEL_Q4 16U
#define LCDM_SERVO_STATIC_LOCK_ERROR 18U
#define LCDM_SERVO_STATIC_LOCK_FULL_ERROR 96U
#define LCDM_SERVO_STATIC_LOCK_MIN_DUTY 520U
#define LCDM_SERVO_LOCK_BASE_DUTY 300U
#define LCDM_SERVO_LOCK_LIGHT_BASE_DUTY 224U
#define LCDM_SERVO_MICRO_HOLD_BAND 6U
#define LCDM_SERVO_V4_DISTURB_EXTRA_BAND 8U
#define LCDM_SERVO_V4_DISTURB_CONFIRM 2U
#define LCDM_SERVO_V3_PHASE_IDLE 0U
#define LCDM_SERVO_V3_PHASE_HOLD 1U
#define LCDM_SERVO_V3_PHASE_ACCEL 2U
#define LCDM_SERVO_V3_PHASE_TRACK 3U
#define LCDM_SERVO_V3_PHASE_BRAKE 4U
#define LCDM_SERVO_V3_PHASE_RETURN 5U
#define LCDM_SERVO_V3_PHASE_LOCK 6U
#define LCDM_SERVO_V3_EVENT_DRIVE 12U
#define LCDM_SERVO_V3_EVENT_DECEL 13U
#define LCDM_SERVO_V3_EVENT_BRAKE 14U
#define LCDM_SERVO_V3_EVENT_RETURN 15U
#define LCDM_SERVO_V3_EVENT_LOCK 16U
#define LCDM_SERVO_V3_EVENT_RETURN_DAMP 17U
#define LCDM_SERVO_V3_EVENT_CROSS_BRAKE 18U
#define LCDM_SERVO_V3_MIN_BRAKE_BAND 160U
#define LCDM_SERVO_V3_BRAKE_VEL_GAIN 3U
#define LCDM_SERVO_V3_BRAKE_MAX_BAND 620U
#define LCDM_SERVO_V3_BRAKE_RELEASE_VEL_Q4 28
#define LCDM_SERVO_V3_BRAKE_MIN_VEL_Q4 32
#define LCDM_SERVO_V3_BRAKE_HOLD_VEL_Q4 68
#define LCDM_SERVO_V3_MOTION_HOLD_BAND 16U
#define LCDM_SERVO_V3_MOTION_HOLD_VEL_Q4 28
#define LCDM_SERVO_V3_RETURN_MIN_DUTY 80U
#define LCDM_SERVO_V3_RETURN_MAX_DUTY 360U
#define LCDM_SERVO_V3_RETURN_NEAR_DUTY 105U
#define LCDM_SERVO_V3_BRAKE_REVERSE_MAX_DUTY 260U
#define LCDM_SERVO_V3_RETURN_DAMP_BASE_BAND 18U
#define LCDM_SERVO_V3_RETURN_DAMP_VEL_DIV 3U
#define LCDM_SERVO_V3_RETURN_DAMP_MAX_BAND 92U
#define LCDM_SERVO_V3_RETURN_DAMP_MIN_VEL_Q4 24
#define LCDM_SERVO_V3_CROSS_BRAKE_MAX_BAND 220U
#define LCDM_SERVO_V3_CROSS_BRAKE_MIN_VEL_Q4 48
#define LCDM_SERVO_V3_LOCK_MAX_DUTY 930U
#define LCDM_SERVO_V6_SETTLE_COUNT 2U
#define LCDM_SERVO_V6_BRAKE_CONFIRM_COUNT 2U
#define LCDM_SERVO_V6_LOCK_RELEASE_COUNT 3U
#define LCDM_SERVO_V6_LOCK_GAIN 8U
#define LCDM_SERVO_V6_BRAKE_VEL_DUTY_DIV 3U
#define LCDM_SERVO_V6_DRIVE_ERR_GAIN 2U
#define LCDM_SERVO_V7_DRIVE_ERR_DIV 4U
#define LCDM_SERVO_V7_LOAD_CONFIRM_COUNT 30U
#define LCDM_SERVO_TARGET_ADC_VDD_MV 3300U
#define LCDM_SERVO_PD_P_SHIFT 7U
#define LCDM_SERVO_PD_D_SHIFT 4U
#define LCDM_SERVO_ADC_LOST_LOW_COUNT 30U
#define LCDM_SERVO_ADC_LOST_HIGH_COUNT 4065U
#define LCDM_SERVO_ADC_LOST_TRIP_COUNT 6U
#if SERVO_ENABLE_INTERNAL_STEP_TEST
#define LCDM_SERVO_FW_TAG " V7.10R4"
#else
#define LCDM_SERVO_FW_TAG " V7.10N"
#endif
#define LCDM_REG_PARAM_VMAX 0U
#define LCDM_REG_PARAM_ACCEL 1U
#define LCDM_REG_PARAM_FF 2U
#define LCDM_REG_PARAM_KV 3U
#define LCDM_REG_PARAM_SLEW 4U
#define LCDM_BRAKE_PARAM_MIN_BAND 0U
#define LCDM_BRAKE_PARAM_VEL_GAIN 1U
#define LCDM_BRAKE_PARAM_MAX_BAND 2U
#define LCDM_BRAKE_PARAM_MIN_VEL 3U
#define LCDM_BRAKE_PARAM_HOLD_VEL 4U
#define LCDM_HOLD_PARAM_BAND 0U
#define LCDM_HOLD_PARAM_VEL 1U
#define LCDM_HOLD_PARAM_RELEASE 2U
#define LCDM_HOLD_PARAM_LOCK_MIN 3U
#define LCDM_HOLD_PARAM_LOCK_MAX 4U
#ifndef LCDM_TJC_BAUD
#define LCDM_TJC_BAUD           9600U
#endif
#ifndef LCDM_TJC_RECOVERY_BAUD
#define LCDM_TJC_RECOVERY_BAUD  38400U
#endif

typedef enum
{
  LCDM_PAGE_MAIN = 0,
  LCDM_PAGE_MENU = 1,
  LCDM_PAGE_OUTPUT = 10,
  LCDM_PAGE_STEP = 13,
  LCDM_PAGE_CAL = 20,
  LCDM_PAGE_REG = 30,
  LCDM_PAGE_DRIVER = 40,
  LCDM_PAGE_DIAG = 50,
  LCDM_PAGE_STARTUP = 60,
  LCDM_PAGE_TEMPLATE = 70,
  LCDM_PAGE_SAVE = 80,
  LCDM_PAGE_WIRING = 90,
} LcdmRuntimePage;

typedef struct
{
  const char *name;
  const char *unit;
  int32_t value;
  int32_t min;
  int32_t max;
  int32_t step;
} LcdmRuntimeParam;

typedef struct
{
  uint16_t x;
  uint16_t y;
  uint8_t event;
} LcdmRuntimeTouch;

typedef struct
{
  LcdmRuntimePage page;
  uint8_t selected;
  uint16_t monitor_step;
  uint16_t touch_count;
  uint16_t last_x;
  uint16_t last_y;
  uint8_t menu_pressed_row;
  char status[32];
} LcdmRuntimeState;

typedef enum
{
  LCDM_PWM_RANGE_IN = 0,
  LCDM_PWM_RANGE_LOW = 1,
  LCDM_PWM_RANGE_HIGH = 2,
} LcdmPwmRangeState;

static LcdmRuntimeParam s_lcdm_step_params[] = {
  {"Bmin", "cnt", LCDM_SERVO_V3_MIN_BRAKE_BAND, 0, 300, 4},
  {"Bg", "", LCDM_SERVO_V3_BRAKE_VEL_GAIN, 0, 10, 1},
  {"Bmax", "cnt", LCDM_SERVO_V3_BRAKE_MAX_BAND, 40, 700, 10},
  {"Bv", "q4", LCDM_SERVO_V3_BRAKE_MIN_VEL_Q4, 0, 160, 4},
  {"Bhd", "q4", LCDM_SERVO_V3_BRAKE_HOLD_VEL_Q4, 0, 200, 4},
};

static LcdmRuntimeParam s_lcdm_cal_params[] = {
  {"Lo", "mV", LCDM_SERVO_WORK_LOW_MV, 150, 1650, 1},
  {"Md", "mV", LCDM_SERVO_WORK_MID_MV, 150, 3200, 1},
  {"Hi", "mV", LCDM_SERVO_WORK_HIGH_MV, 1650, 3200, 1},
  {"InL", "us", 500, 300, 1499, 1},
  {"InH", "us", 2500, 1501, 2699, 1},
};

static LcdmRuntimeParam s_lcdm_reg_params[] = {
  {"Vmax", "q4", LCDM_SERVO_TRAJ_MAX_VEL_Q4, 120, 800, 8},
  {"Acc", "q4", LCDM_SERVO_TRAJ_ACCEL_Q4, 8, 120, 2},
  {"FF", "", LCDM_SERVO_MOVING_FF_DUTY, 0, 400, 5},
  {"Kv", "", LCDM_SERVO_VEL_K_NUM, 1, 10, 1},
  {"Slew", "", LCDM_SERVO_OUT_SLEW, 20, 240, 5},
};

static LcdmRuntimeParam s_lcdm_driver_params[] = {
  {"Inv", "", 0, 0, 1, 1},
  {"Dir", "", 1, 0, 1, 1},
  {"Hz", "Hz", 1000, 330, 1000, 670},
  {"Max", "", 1000, 0, 1000, 5},
  {"Sta", "", 1000, 0, 1000, 5},
  {"Stm", "ms", 600, 0, 3000, 10},
};

static LcdmRuntimeParam s_lcdm_startup_params[] = {
  {"Rng", "", LCDM_SERVO_RANGE_LOCK, LCDM_SERVO_RANGE_RELEASE, LCDM_SERVO_RANGE_LOCK, 1},
  {"Dly", "ms", 300, 0, 3000, 10},
  {"Stb", "cnt", 4, 1, 10, 1},
  {"Bd", "us", 0, 0, 50, 1},
  {"CDB", "cnt", 1, 0, 120, 1},
};

static LcdmRuntimeParam s_lcdm_template_params[] = {
  {"Hbd", "cnt", LCDM_SERVO_V3_MOTION_HOLD_BAND, 6, 80, 1},
  {"Hvel", "q4", LCDM_SERVO_V3_MOTION_HOLD_VEL_Q4, 0, 120, 2},
  {"Rel", "cnt", LCDM_SERVO_HOLD_RELEASE_BAND, 6, 160, 1},
  {"Lmin", "", LCDM_SERVO_LOCK_LIGHT_BASE_DUTY, 0, 800, 10},
  {"Lmax", "", LCDM_SERVO_V3_LOCK_MAX_DUTY, 100, 1000, 10},
};

static const char *const s_lcdm_menu_text[] = {
  "P10",
  "P31",
  "P20",
  "P30",
  "P40",
  "P50",
  "P60",
  "P70",
  "P80",
  "P90",
};

static uint16_t s_lcdm_last_pwm_pulse_us = 0xFFFFU;
static uint16_t s_lcdm_last_pwm_control_us = 0xFFFFU;
static uint32_t s_lcdm_last_pwm_period_us = 0xFFFFFFFFUL;
static uint16_t s_lcdm_last_servo_target_adc = 0xFFFFU;
static uint16_t s_lcdm_last_servo_target_mv = 0xFFFFU;
static uint16_t s_lcdm_last_servo_feedback_adc = 0xFFFFU;
static int16_t s_lcdm_last_servo_error_adc = 32767;
static int16_t s_lcdm_last_pwm_duty = 32767;
static int8_t s_lcdm_last_pwm_direction = 127;
static uint8_t s_lcdm_last_pwm_state = 0xFFU;
static uint16_t s_lcdm_last_hbridge_frequency_hz;
static int16_t s_lcdm_servo_current_duty;
static int8_t s_lcdm_servo_output_dir;
static uint32_t s_lcdm_servo_reverse_dead_until_ms;
static uint8_t s_lcdm_servo_reverse_dead_active;
static int16_t s_lcdm_servo_current_velocity;
static int16_t s_lcdm_servo_ref_velocity;
static uint8_t s_lcdm_servo_current_state;
static LcdmPwmRangeState s_lcdm_servo_current_range = LCDM_PWM_RANGE_IN;
static uint16_t s_lcdm_servo_prev_feedback_adc;
static uint16_t s_lcdm_servo_prev_target_adc;
static uint16_t s_lcdm_servo_dir_bad_count;
static uint8_t s_lcdm_servo_dir_alarm;
static int32_t s_lcdm_servo_prev_motor_invert = -1;
static uint16_t s_lcdm_servo_filtered_feedback_adc;
static uint16_t s_lcdm_servo_last_control_feedback_adc;
static uint16_t s_lcdm_servo_last_target_adc = 0xFFFFU;
static uint16_t s_lcdm_servo_last_control_pulse_us = 0xFFFFU;
static uint16_t s_lcdm_servo_velocity_feedback_adc = 0xFFFFU;
static uint16_t s_lcdm_servo_latched_pulse_us = LCDM_SERVO_INPUT_MID_US;
static uint16_t s_lcdm_servo_candidate_pulse_us = LCDM_SERVO_INPUT_MID_US;
static uint32_t s_lcdm_servo_last_processed_pulse_count;
static uint8_t s_lcdm_servo_latched_pulse_seeded;
static uint8_t s_lcdm_servo_candidate_count;
static uint8_t s_lcdm_servo_filter_seeded;
static uint8_t s_lcdm_servo_hold_active;
static uint8_t s_lcdm_servo_cmd_sensitive;
static uint16_t s_lcdm_last_servo_pwm_window = 0xFFFFU;
static uint16_t s_lcdm_servo_adc_window_min = 0xFFFFU;
static uint16_t s_lcdm_servo_adc_window_max;
static uint16_t s_lcdm_last_servo_adc_window = 0xFFFFU;
static uint16_t s_lcdm_servo_overshoot_adc;
static uint16_t s_lcdm_last_servo_overshoot_adc = 0xFFFFU;
static int16_t s_lcdm_last_servo_velocity = 32767;
static uint8_t s_lcdm_last_servo_event = 0xFFU;
static int8_t s_lcdm_servo_move_dir;
static int8_t s_lcdm_servo_motion_dir;
static uint8_t s_lcdm_servo_adc_lost_count;
static uint8_t s_lcdm_servo_hold_deviation_count;
static uint8_t s_lcdm_servo_adc_jump_count;
static uint16_t s_lcdm_servo_last_motion_error = 0xFFFFU;
static uint16_t s_lcdm_servo_motion_start_error;
static uint8_t s_lcdm_servo_motion_settle_count;
static uint8_t s_lcdm_servo_approach_brake_tail_count;
static uint8_t s_lcdm_servo_v3_phase;
static uint16_t s_lcdm_servo_hold_anchor_adc;
static uint8_t s_lcdm_pwm_source = LCDM_PWM_SOURCE_EXTERNAL;
static uint32_t s_lcdm_runtime_pwm_start_ms;
static uint16_t s_lcdm_runtime_pwm_seed_us = LCDM_SERVO_INPUT_MID_US;
static uint8_t s_lcdm_runtime_pwm_seeded;
static int32_t s_lcdm_servo_pos_q4;
static int32_t s_lcdm_servo_vel_q4;
static int32_t s_lcdm_servo_traj_pos_q4;
static int32_t s_lcdm_servo_traj_vel_q4;
static uint16_t s_lcdm_servo_cmd_target_adc = LCDM_ADC_TEST_CENTER_RAW;
static uint8_t s_lcdm_servo_ab_seeded;

static uint16_t lcdm_param_u16(const LcdmRuntimeParam *params,
                               uint8_t index,
                               uint16_t fallback)
{
  int32_t value = params[index].value;
  if (value < 0)
  {
    return fallback;
  }
  if (value > 65535)
  {
    return fallback;
  }
  return (uint16_t)value;
}

static int32_t lcdm_param_i32(const LcdmRuntimeParam *params,
                              uint8_t index,
                              int32_t fallback)
{
  int32_t value = params[index].value;
  if ((value < params[index].min) || (value > params[index].max))
  {
    return fallback;
  }
  return value;
}

typedef struct __attribute__((packed, aligned(4)))
{
  uint32_t seq_head;
  uint32_t time_ms;
  uint16_t pwm_us;
  uint16_t pwm_raw_us;
  uint16_t pwm_period_us;
  uint16_t raw_adc;
  uint16_t target_adc;
  uint16_t feedback_adc;
  int16_t error_adc;
  int16_t duty;
  int16_t velocity_q4;
  int16_t ref_velocity;
  int16_t traj_adc;
  int16_t traj_vel_q4;
  uint16_t overshoot_adc;
  uint16_t pwm_candidate_us;
  uint8_t state;
  uint8_t event;
  uint8_t pwm_candidate_count;
  uint8_t reserved;
  uint32_t seq_tail;
} ServoCurveFrame;

#define SERVO_CURVE_LOG_CAPACITY 35U
#define SERVO_CURVE_LOG_CAPTURE_MS 900U

typedef struct __attribute__((packed, aligned(4)))
{
  uint32_t time_ms;
  uint16_t pwm_us;
  uint16_t target_adc;
  uint16_t feedback_adc;
  int16_t error_adc;
  int16_t duty;
  int16_t velocity_q4;
  uint8_t state;
  uint8_t event;
} ServoCurveLogEntry;

volatile ServoCurveFrame g_servo_curve_frame;
volatile ServoCurveLogEntry g_servo_curve_log[SERVO_CURVE_LOG_CAPACITY];
volatile uint16_t g_servo_curve_log_head;
volatile uint16_t g_servo_curve_log_count;
volatile uint32_t g_servo_curve_time_ms;
volatile uint16_t g_servo_curve_pwm_us;
volatile uint16_t g_servo_curve_pwm_raw_us;
volatile uint16_t g_servo_curve_pwm_period_us;
volatile uint16_t g_servo_curve_raw_adc;
volatile uint16_t g_servo_curve_target_adc;
volatile uint16_t g_servo_curve_feedback_adc;
volatile int16_t g_servo_curve_error_adc;
volatile int16_t g_servo_curve_duty;
volatile int16_t g_servo_curve_velocity_q4;
volatile int16_t g_servo_curve_ref_velocity;
volatile int16_t g_servo_curve_traj_adc;
volatile int16_t g_servo_curve_traj_vel_q4;
volatile uint16_t g_servo_curve_overshoot_adc;
volatile uint16_t g_servo_curve_pwm_candidate_us;
volatile uint8_t g_servo_curve_state;
volatile uint8_t g_servo_curve_event;
volatile uint8_t g_servo_curve_pwm_candidate_count;
volatile uint32_t g_servo_curve_seq_head;
volatile uint32_t g_servo_curve_seq_tail;
static uint32_t s_servo_curve_log_until_ms;
static uint32_t s_servo_curve_log_generation;
static uint8_t s_servo_curve_log_active;
static uint8_t s_servo_curve_log_force;
static uint8_t s_servo_curve_static_lock_logged;

static int16_t lcdm_clamp_i32_to_i16(int32_t value)
{
  if (value > 32767)
  {
    return 32767;
  }
  if (value < -32768)
  {
    return -32768;
  }
  return (int16_t)value;
}

static void lcdm_servo_curve_log_trigger(uint32_t now_ms)
{
  g_servo_curve_log_head = 0U;
  g_servo_curve_log_count = 0U;
  s_servo_curve_log_until_ms = now_ms + SERVO_CURVE_LOG_CAPTURE_MS;
  s_servo_curve_log_active = 1U;
  s_servo_curve_log_force = 1U;
  s_servo_curve_log_generation++;
}

static void lcdm_servo_curve_publish(uint16_t pwm_us,
                                     uint16_t raw_adc,
                                     uint16_t target_adc,
                                     uint16_t feedback_adc,
                                     int16_t error_adc,
                                     int16_t duty,
                                     uint8_t event)
{
  static uint32_t seq;
  static uint32_t last_log_ms;
  static uint32_t last_log_generation;
  static uint16_t last_log_target_adc = 0xFFFFU;
  static int16_t last_log_duty;
  static uint8_t last_log_event = 0xFFU;
  uint32_t updating_seq = seq + 1U;
  uint32_t ready_seq = seq + 2U;
  uint32_t time_ms = HAL_GetTick();
#if !SERVO_ENABLE_INTERNAL_STEP_TEST
  PWM_InputSample pwm_sample = PWM_Input_GetSample();
#endif
  int16_t velocity_q4 = lcdm_clamp_i32_to_i16(s_lcdm_servo_vel_q4);
  int8_t duty_dir = (duty > 0) ? 1 : ((duty < 0) ? -1 : 0);
  int8_t last_duty_dir = (last_log_duty > 0) ? 1 : ((last_log_duty < 0) ? -1 : 0);
  bool capture_active = (s_servo_curve_log_active != 0U);
  bool log_now;

  if ((last_log_generation != s_servo_curve_log_generation) || (s_servo_curve_log_force != 0U))
  {
    last_log_ms = 0U;
    last_log_target_adc = 0xFFFFU;
    last_log_duty = 0;
    last_log_event = 0xFFU;
    last_log_generation = s_servo_curve_log_generation;
  }
  if (capture_active && ((int32_t)(time_ms - s_servo_curve_log_until_ms) > 0))
  {
    s_servo_curve_log_active = 0U;
    capture_active = false;
  }

  log_now = (s_servo_curve_log_force != 0U) ||
                 ((uint32_t)(time_ms - last_log_ms) >= 20U) ||
                 (target_adc != last_log_target_adc) ||
                 (duty_dir != last_duty_dir) ||
                 (((duty < 0) ? -duty : duty) >=
                  (int16_t)(((last_log_duty < 0) ? -last_log_duty : last_log_duty) + 160)) ||
                 (((last_log_duty < 0) ? -last_log_duty : last_log_duty) >=
                  (int16_t)(((duty < 0) ? -duty : duty) + 160)) ||
                 (event != last_log_event);

  g_servo_curve_frame.seq_head = updating_seq;
  g_servo_curve_frame.seq_tail = updating_seq;
  g_servo_curve_frame.time_ms = time_ms;
  g_servo_curve_frame.pwm_us = pwm_us;
#if SERVO_ENABLE_INTERNAL_STEP_TEST
  g_servo_curve_frame.pwm_raw_us = pwm_us;
  g_servo_curve_frame.pwm_period_us = 20000U;
  g_servo_curve_frame.pwm_candidate_us = pwm_us;
  g_servo_curve_frame.pwm_candidate_count = 0U;
#else
  g_servo_curve_frame.pwm_raw_us = pwm_sample.raw_pulse_us;
  g_servo_curve_frame.pwm_period_us = (uint16_t)pwm_sample.period_us;
  g_servo_curve_frame.pwm_candidate_us = pwm_sample.candidate_pulse_us;
  g_servo_curve_frame.pwm_candidate_count = pwm_sample.candidate_count;
#endif
  g_servo_curve_frame.raw_adc = raw_adc;
  g_servo_curve_frame.target_adc = target_adc;
  g_servo_curve_frame.feedback_adc = feedback_adc;
  g_servo_curve_frame.error_adc = error_adc;
  g_servo_curve_frame.duty = duty;
  g_servo_curve_frame.velocity_q4 = velocity_q4;
  g_servo_curve_frame.state = s_lcdm_servo_current_state;
  g_servo_curve_frame.event = event;
  g_servo_curve_frame.seq_tail = ready_seq;
  g_servo_curve_frame.seq_head = ready_seq;
  seq = ready_seq;

  if (capture_active && log_now)
  {
    uint16_t head = g_servo_curve_log_head;
    volatile ServoCurveLogEntry *entry = &g_servo_curve_log[head];
    entry->time_ms = time_ms;
    entry->pwm_us = pwm_us;
    entry->target_adc = target_adc;
    entry->feedback_adc = feedback_adc;
    entry->error_adc = error_adc;
    entry->duty = duty;
    entry->velocity_q4 = velocity_q4;
    entry->state = s_lcdm_servo_current_state;
    entry->event = event;
    head++;
    if (head >= SERVO_CURVE_LOG_CAPACITY)
    {
      head = 0U;
    }
    g_servo_curve_log_head = head;
    if (g_servo_curve_log_count < SERVO_CURVE_LOG_CAPACITY)
    {
      g_servo_curve_log_count++;
    }
    last_log_ms = time_ms;
    last_log_target_adc = target_adc;
    last_log_duty = duty;
    last_log_event = event;
    s_servo_curve_log_force = 0U;
  }
}

static void lcdm_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  char cmd[48];
  (void)snprintf(cmd, sizeof(cmd), "fill %u,%u,%u,%u,%u", x, y, w, h, color);
  pa7_tjc_write_cmd(cmd);
}

static void lcdm_text(uint16_t x,
                      uint16_t y,
                      uint16_t w,
                      uint16_t h,
                      uint16_t color,
                      uint16_t bg,
                      uint8_t align,
                      const char *text)
{
  char cmd[320];
  (void)snprintf(cmd,
                 sizeof(cmd),
                 "xstr %u,%u,%u,%u,%u,%u,%u,%u,1,1,\"%s\"",
                 x,
                 y,
                 w,
                 h,
                 LCDM_TJC_FONT_ID,
                 color,
                 bg,
                 align,
                 text);
  pa7_tjc_write_cmd(cmd);
}

static void lcdm_button(uint16_t x, uint16_t w, const char *text, uint16_t bg)
{
  lcdm_fill(x, LCDM_FOOTER_Y, w, LCDM_FOOTER_H, bg);
  lcdm_text((uint16_t)(x + 2U), (uint16_t)(LCDM_FOOTER_Y + 5U), (uint16_t)(w - 4U), 22U, LCDM_COLOR_WHITE, bg, 1U, text);
}

static void lcdm_footer(const char *left, const char *mid, const char *right)
{
  lcdm_button(0U, LCDM_FOOTER_SEG_W, left, LCDM_COLOR_DARK_GRAY);
  lcdm_button(LCDM_FOOTER_SEG_W, LCDM_FOOTER_SEG_W, mid, LCDM_COLOR_NAVY);
  lcdm_button((uint16_t)(LCDM_FOOTER_SEG_W * 2U), LCDM_FOOTER_SEG_W, right, LCDM_COLOR_DARK_GRAY);
}

static void lcdm_page_batch_begin(void)
{
  pa7_tjc_write_cmd("ref_stop");
}

static void lcdm_page_batch_end(void)
{
  pa7_tjc_write_cmd("ref_star");
}

static void lcdm_page_base(const char *title, const char *status)
{
  pa7_tjc_write_cmd("cls 65535");
#if SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
  lcdm_runtime_wait_ms_with_control(LCDM_TJC_PAGE_CLEAR_MS);
#else
  wait_systick_ticks((SystemCoreClock / 1000U) * LCDM_TJC_PAGE_CLEAR_MS);
#endif
  lcdm_fill(0U, 0U, LCDM_W, LCDM_TITLE_H, LCDM_COLOR_NAVY);
  lcdm_text(6U, 3U, 340U, 26U, LCDM_COLOR_WHITE, LCDM_COLOR_NAVY, 0U, title);
  lcdm_text(350U, 3U, 124U, 26U, LCDM_COLOR_WHITE, LCDM_COLOR_NAVY, 2U, status);
}

static const char *lcdm_input_mode_text(void)
{
#if SERVO_ENABLE_INTERNAL_STEP_TEST
  return "STEP";
#else
  return (s_lcdm_pwm_source == LCDM_PWM_SOURCE_EXTERNAL) ? "EXTPWM" : "INTPWM";
#endif
}

static uint16_t lcdm_row_bg(uint8_t row)
{
  return (row & 1U) ? LCDM_COLOR_WHITE : LCDM_COLOR_ROW_ALT;
}

static void lcdm_row_ex(uint8_t row, const char *name, const char *value, uint16_t value_color, bool selected)
{
  uint16_t y = (uint16_t)(LCDM_ROW_Y + ((uint16_t)row * LCDM_ROW_STEP));
  uint16_t bg = lcdm_row_bg(row);
  uint16_t name_color = selected ? LCDM_COLOR_RED : LCDM_COLOR_BLUE;

  lcdm_fill(LCDM_ROW_BG_X, (uint16_t)(y - 2U), LCDM_ROW_W, (uint16_t)(LCDM_ROW_H + 2U), bg);
  lcdm_text(LCDM_ROW_X, y, LCDM_ROW_NAME_W, LCDM_ROW_H, name_color, bg, 0U, name);
  lcdm_text(LCDM_ROW_VALUE_X, y, LCDM_ROW_VALUE_W, LCDM_ROW_H, value_color, bg, 0U, value);
}

static void lcdm_row(uint8_t row, const char *name, const char *value, uint16_t value_color)
{
  lcdm_row_ex(row, name, value, value_color, false);
}

static void lcdm_value_cell(uint8_t row, const char *value, uint16_t value_color)
{
  uint16_t y = (uint16_t)(LCDM_ROW_Y + ((uint16_t)row * LCDM_ROW_STEP));
  uint16_t bg = lcdm_row_bg(row);

  lcdm_text(LCDM_ROW_VALUE_X, y, LCDM_ROW_VALUE_W, LCDM_ROW_H, value_color, bg, 0U, value);
}

static void lcdm_menu_row_ex(uint8_t row, const char *text, bool pressed)
{
  uint8_t grid_row = (uint8_t)(row % LCDM_MENU_ROWS);
  uint8_t grid_col = (uint8_t)(row / LCDM_MENU_ROWS);
  uint16_t x = (uint16_t)(LCDM_MENU_GRID_X + ((uint16_t)grid_col * LCDM_MENU_COL_STEP));
  uint16_t y = (uint16_t)(LCDM_MENU_GRID_Y + ((uint16_t)grid_row * LCDM_MENU_ROW_STEP));
  uint16_t bg = lcdm_row_bg(grid_row);
  uint16_t fg = pressed ? LCDM_COLOR_RED : LCDM_COLOR_BLACK;

  lcdm_text((uint16_t)(x + 8U),
            (uint16_t)(y + 4U),
            (uint16_t)(LCDM_MENU_CELL_W - 16U),
            28U,
            fg,
            bg,
            1U,
            text);
}

static void lcdm_menu_grid_background(void)
{
  for (uint8_t row = 0U; row < LCDM_MENU_ROWS; row++)
  {
    uint16_t y = (uint16_t)(LCDM_MENU_GRID_Y + ((uint16_t)row * LCDM_MENU_ROW_STEP));
    lcdm_fill(LCDM_ROW_BG_X, (uint16_t)(y - 2U), LCDM_ROW_W, (uint16_t)(LCDM_MENU_CELL_H + 2U), lcdm_row_bg(row));
  }
}

static void lcdm_menu_row(uint8_t row, const char *text)
{
  lcdm_menu_row_ex(row, text, false);
}

static void lcdm_param_row(uint8_t row, const LcdmRuntimeParam *param, bool selected)
{
  char text[36];

  if ((param->unit != NULL) && (param->unit[0] != '\0'))
  {
    (void)snprintf(text, sizeof(text), "%ld %s", (long)param->value, param->unit);
  }
  else
  {
    (void)snprintf(text, sizeof(text), "%ld", (long)param->value);
  }

  lcdm_row_ex(row, param->name, text, selected ? LCDM_COLOR_RED : LCDM_COLOR_BLUE, selected);
}

static void lcdm_adjust_param(LcdmRuntimeParam *params, uint8_t count, uint8_t selected, int8_t direction)
{
  int32_t value;

  if (selected >= count)
  {
    return;
  }

  value = params[selected].value + ((int32_t)direction * params[selected].step);
  if (value < params[selected].min)
  {
    value = params[selected].min;
  }
  else if (value > params[selected].max)
  {
    value = params[selected].max;
  }
  params[selected].value = value;
}

static void lcdm_draw_param_page(const char *title,
                                 const char *status,
                                 LcdmRuntimeParam *params,
                                 uint8_t count,
                                 uint8_t selected)
{
  lcdm_page_base(title, status);
  for (uint8_t i = 0U; (i < count) && (i < LCDM_VISIBLE_ROWS); i++)
  {
    lcdm_param_row(i, &params[i], i == selected);
  }
  lcdm_footer("-", "BK", "+");
}

static void lcdm_draw_p01(void)
{
  lcdm_page_base("P01", "SEL");
  lcdm_menu_grid_background();
  for (uint8_t i = 0U; (i < LCDM_ARRAY_COUNT(s_lcdm_menu_text)) && (i < LCDM_MENU_VISIBLE_COUNT); i++)
  {
    lcdm_menu_row(i, s_lcdm_menu_text[i]);
  }
  lcdm_footer("MN", "BK", "ENT");
}

static bool lcdm_pwm_sample_is_fresh(const PWM_InputSample *sample)
{
  const ServoParams *params = Servo_Params_Get();

  return sample->valid &&
         ((uint32_t)(HAL_GetTick() - sample->last_edge_ms) <= params->input_timeout_ms);
}

static bool lcdm_pwm_runtime_internal_enabled(void)
{
#if SERVO_ENABLE_INTERNAL_STEP_TEST
  return true;
#else
  return (s_lcdm_pwm_source != LCDM_PWM_SOURCE_EXTERNAL);
#endif
}

static void lcdm_runtime_pwm_test_sample(PWM_InputSample *sample)
{
  uint32_t now = HAL_GetTick();
  uint32_t elapsed;
  uint32_t phase;
  uint16_t pulse;
#if SERVO_ENABLE_INTERNAL_STEP_TEST
  static uint16_t s_runtime_last_pulse = 0xFFFFU;
  static uint32_t s_runtime_pulse_count;
#endif

  if (s_lcdm_runtime_pwm_seeded == 0U)
  {
    s_lcdm_runtime_pwm_seed_us = (s_lcdm_servo_last_control_pulse_us != 0xFFFFU) ?
                                 s_lcdm_servo_last_control_pulse_us :
                                 LCDM_SERVO_INPUT_MID_US;
    s_lcdm_runtime_pwm_start_ms = now;
    s_lcdm_runtime_pwm_seeded = 1U;
  }

  elapsed = now - s_lcdm_runtime_pwm_start_ms;
  pulse = s_lcdm_runtime_pwm_seed_us;
  if (elapsed >= 800UL)
  {
    phase = ((elapsed - 800UL) / 1200UL) % 6UL;
    if ((phase == 0UL) || (phase == 4UL))
    {
      pulse = 1000U;
    }
    else if ((phase == 2UL) || (phase == 5UL))
    {
      pulse = 2000U;
    }
  }

  sample->pulse_us = pulse;
  sample->raw_pulse_us = pulse;
  sample->candidate_pulse_us = pulse;
  sample->candidate_count = 0U;
  sample->period_us = 20000UL;
  sample->last_edge_ms = now;
  sample->edge_count = now;
#if SERVO_ENABLE_INTERNAL_STEP_TEST
  if (pulse != s_runtime_last_pulse)
  {
    s_runtime_last_pulse = pulse;
    s_runtime_pulse_count++;
  }
  sample->pulse_count = s_runtime_pulse_count;
#else
  sample->pulse_count = now / 20UL;
#endif
  sample->raw_high = false;
  sample->valid = true;
}

#if SERVO_ENABLE_INTERNAL_STEP_TEST
static void lcdm_internal_step_test_sample(PWM_InputSample *sample)
{
  lcdm_runtime_pwm_test_sample(sample);
}
#endif

static void lcdm_reset_pwm_live_cache(void)
{
  s_lcdm_last_pwm_pulse_us = 0xFFFFU;
  s_lcdm_last_pwm_control_us = 0xFFFFU;
  s_lcdm_last_pwm_period_us = 0xFFFFFFFFUL;
  s_lcdm_last_servo_target_adc = 0xFFFFU;
  s_lcdm_last_servo_target_mv = 0xFFFFU;
  s_lcdm_last_servo_feedback_adc = 0xFFFFU;
  s_lcdm_last_servo_error_adc = 32767;
  s_lcdm_last_pwm_duty = 32767;
  s_lcdm_last_pwm_direction = 127;
  s_lcdm_last_pwm_state = 0xFFU;
  s_lcdm_servo_current_duty = 0;
  s_lcdm_servo_output_dir = 0;
  s_lcdm_servo_reverse_dead_until_ms = 0U;
  s_lcdm_servo_reverse_dead_active = 0U;
  s_lcdm_servo_current_velocity = 0;
  s_lcdm_servo_ref_velocity = 0;
  s_lcdm_servo_current_state = LCDM_SERVO_STATE_NO_PWM;
  s_lcdm_servo_current_range = LCDM_PWM_RANGE_IN;
  s_lcdm_servo_prev_feedback_adc = 0xFFFFU;
  s_lcdm_servo_prev_target_adc = 0xFFFFU;
  s_lcdm_servo_dir_bad_count = 0U;
  s_lcdm_servo_dir_alarm = 0U;
  s_lcdm_servo_prev_motor_invert = s_lcdm_driver_params[0].value;
  s_lcdm_servo_filtered_feedback_adc = 0U;
  s_lcdm_servo_last_control_feedback_adc = 0U;
  s_lcdm_servo_last_target_adc = 0xFFFFU;
  s_lcdm_servo_last_control_pulse_us = 0xFFFFU;
  s_lcdm_servo_velocity_feedback_adc = 0xFFFFU;
  s_lcdm_servo_latched_pulse_us = LCDM_SERVO_INPUT_MID_US;
  s_lcdm_servo_candidate_pulse_us = LCDM_SERVO_INPUT_MID_US;
  s_lcdm_servo_last_processed_pulse_count = 0UL;
  s_lcdm_servo_latched_pulse_seeded = 0U;
  s_lcdm_servo_candidate_count = 0U;
  s_lcdm_servo_filter_seeded = 0U;
  s_lcdm_servo_hold_active = 0U;
  s_lcdm_servo_cmd_sensitive = 0U;
  s_lcdm_last_servo_pwm_window = 0xFFFFU;
  s_lcdm_servo_adc_window_min = 0xFFFFU;
  s_lcdm_servo_adc_window_max = 0U;
  s_lcdm_last_servo_adc_window = 0xFFFFU;
  s_lcdm_servo_overshoot_adc = 0U;
  s_lcdm_last_servo_overshoot_adc = 0xFFFFU;
  s_lcdm_last_servo_velocity = 32767;
  s_lcdm_last_servo_event = 0xFFU;
  s_lcdm_servo_move_dir = 0;
  s_lcdm_servo_motion_dir = 0;
  s_lcdm_servo_adc_lost_count = 0U;
  s_lcdm_servo_hold_deviation_count = 0U;
  s_lcdm_servo_adc_jump_count = 0U;
  s_lcdm_servo_last_motion_error = 0xFFFFU;
  s_lcdm_servo_motion_start_error = 0U;
  s_lcdm_servo_motion_settle_count = 0U;
  s_lcdm_servo_approach_brake_tail_count = 0U;
  s_lcdm_servo_v3_phase = LCDM_SERVO_V3_PHASE_IDLE;
  s_lcdm_servo_hold_anchor_adc = LCDM_ADC_TEST_CENTER_RAW;
  s_lcdm_servo_pos_q4 = 0;
  s_lcdm_servo_vel_q4 = 0;
  s_lcdm_servo_traj_pos_q4 = 0;
  s_lcdm_servo_traj_vel_q4 = 0;
  s_lcdm_servo_cmd_target_adc = LCDM_ADC_TEST_CENTER_RAW;
  s_lcdm_servo_ab_seeded = 0U;
  s_lcdm_runtime_pwm_start_ms = HAL_GetTick();
  s_lcdm_runtime_pwm_seed_us = LCDM_SERVO_INPUT_MID_US;
  s_lcdm_runtime_pwm_seeded = 0U;
}

static uint16_t lcdm_clamp_u16(uint32_t value, uint16_t min_value, uint16_t max_value)
{
  if (value < min_value)
  {
    return min_value;
  }
  if (value > max_value)
  {
    return max_value;
  }
  return (uint16_t)value;
}

static uint16_t lcdm_mv_to_adc(uint16_t mv, uint16_t vdd_mv)
{
  if (vdd_mv == 0U)
  {
    vdd_mv = 3300U;
  }
  return lcdm_clamp_u16(((((uint32_t)mv * 4095U) + (vdd_mv / 2U)) / vdd_mv), 0U, 4095U);
}

static LcdmPwmRangeState lcdm_pwm_range_state(uint16_t pulse_us)
{
  uint16_t low_us = (uint16_t)s_lcdm_cal_params[3].value;
  uint16_t high_us = (uint16_t)s_lcdm_cal_params[4].value;

  if (low_us >= LCDM_SERVO_INPUT_MID_US)
  {
    low_us = 500U;
  }
  if (high_us <= LCDM_SERVO_INPUT_MID_US)
  {
    high_us = 2500U;
  }

  if (pulse_us < low_us)
  {
    return LCDM_PWM_RANGE_LOW;
  }
  if (pulse_us > high_us)
  {
    return LCDM_PWM_RANGE_HIGH;
  }
  return LCDM_PWM_RANGE_IN;
}

static uint16_t lcdm_servo_update_latched_pulse(uint16_t pulse_us, uint32_t pulse_count)
{
  uint16_t stable_band = (uint16_t)s_lcdm_startup_params[LCDM_STARTUP_PARAM_INPUT_STABLE_BAND].value;
  uint16_t delta;

  if (stable_band > 50U)
  {
    stable_band = 50U;
  }

  if (s_lcdm_servo_latched_pulse_seeded == 0U)
  {
    s_lcdm_servo_latched_pulse_us = pulse_us;
    s_lcdm_servo_candidate_pulse_us = pulse_us;
    s_lcdm_servo_last_processed_pulse_count = pulse_count;
    s_lcdm_servo_latched_pulse_seeded = 1U;
    s_lcdm_servo_candidate_count = 0U;
    return s_lcdm_servo_latched_pulse_us;
  }

  if (pulse_count == s_lcdm_servo_last_processed_pulse_count)
  {
    return s_lcdm_servo_latched_pulse_us;
  }
  s_lcdm_servo_last_processed_pulse_count = pulse_count;

  if (lcdm_pwm_range_state(pulse_us) != LCDM_PWM_RANGE_IN)
  {
    s_lcdm_servo_latched_pulse_us = pulse_us;
    s_lcdm_servo_candidate_pulse_us = pulse_us;
    s_lcdm_servo_candidate_count = 0U;
    return s_lcdm_servo_latched_pulse_us;
  }

  delta = (pulse_us > s_lcdm_servo_latched_pulse_us) ?
          (uint16_t)(pulse_us - s_lcdm_servo_latched_pulse_us) :
          (uint16_t)(s_lcdm_servo_latched_pulse_us - pulse_us);
  if (delta > stable_band)
  {
    s_lcdm_servo_latched_pulse_us = pulse_us;
    s_lcdm_servo_candidate_pulse_us = pulse_us;
    s_lcdm_servo_candidate_count = 0U;
  }
  else
  {
    s_lcdm_servo_candidate_pulse_us = s_lcdm_servo_latched_pulse_us;
    s_lcdm_servo_candidate_count = 0U;
  }

  return s_lcdm_servo_latched_pulse_us;
}

static uint16_t lcdm_pwm_to_target_mv(uint16_t pulse_us)
{
  uint16_t low_mv = (uint16_t)s_lcdm_cal_params[0].value;
  uint16_t mid_mv = (uint16_t)s_lcdm_cal_params[1].value;
  uint16_t high_mv = (uint16_t)s_lcdm_cal_params[2].value;
  uint16_t low_us = (uint16_t)s_lcdm_cal_params[3].value;
  uint16_t high_us = (uint16_t)s_lcdm_cal_params[4].value;
  uint16_t mid_us = LCDM_SERVO_INPUT_MID_US;
  uint32_t numerator;
  uint32_t denominator;

  if (low_us >= mid_us)
  {
    low_us = 500U;
  }
  if (high_us <= mid_us)
  {
    high_us = 2500U;
  }
  if (mid_mv < low_mv)
  {
    mid_mv = low_mv;
  }
  if (high_mv < mid_mv)
  {
    high_mv = mid_mv;
  }

  if (pulse_us <= low_us)
  {
    return low_mv;
  }
  if (pulse_us >= high_us)
  {
    return high_mv;
  }
  if (pulse_us <= mid_us)
  {
    numerator = (uint32_t)(pulse_us - low_us) * (uint32_t)(mid_mv - low_mv);
    denominator = (uint32_t)(mid_us - low_us);
    return (uint16_t)(low_mv + ((numerator + (denominator / 2U)) / denominator));
  }

  numerator = (uint32_t)(pulse_us - mid_us) * (uint32_t)(high_mv - mid_mv);
  denominator = (uint32_t)(high_us - mid_us);
  return (uint16_t)(mid_mv + ((numerator + (denominator / 2U)) / denominator));
}

static uint16_t lcdm_pwm_to_target_adc(uint16_t pulse_us)
{
  uint16_t target_mv = lcdm_pwm_to_target_mv(pulse_us);
  return lcdm_mv_to_adc(target_mv, LCDM_SERVO_TARGET_ADC_VDD_MV);
}

static void lcdm_servo_adc_window_add(uint16_t raw_adc)
{
  if (raw_adc < s_lcdm_servo_adc_window_min)
  {
    s_lcdm_servo_adc_window_min = raw_adc;
  }
  if (raw_adc > s_lcdm_servo_adc_window_max)
  {
    s_lcdm_servo_adc_window_max = raw_adc;
  }
}

static uint16_t lcdm_servo_adc_window_take(void)
{
  uint16_t window = 0U;
  if ((s_lcdm_servo_adc_window_min != 0xFFFFU) &&
      (s_lcdm_servo_adc_window_max >= s_lcdm_servo_adc_window_min))
  {
    window = (uint16_t)(s_lcdm_servo_adc_window_max - s_lcdm_servo_adc_window_min);
  }
  s_lcdm_servo_adc_window_min = 0xFFFFU;
  s_lcdm_servo_adc_window_max = 0U;
  return window;
}

static bool lcdm_servo_adc_is_valid(uint16_t raw_adc)
{
  return ((raw_adc > LCDM_SERVO_ADC_LOST_LOW_COUNT) &&
          (raw_adc < LCDM_SERVO_ADC_LOST_HIGH_COUNT));
}

static bool lcdm_servo_reverse_alarm_update(uint16_t target_adc,
                                            uint16_t feedback_adc,
                                            int16_t error_adc,
                                            int16_t duty)
{
  int16_t feedback_delta;
  uint16_t abs_error = (error_adc < 0) ? (uint16_t)(-error_adc) : (uint16_t)error_adc;
  uint16_t abs_duty = (duty < 0) ? (uint16_t)(-duty) : (uint16_t)duty;
  uint16_t target_delta;
  bool moving_away = false;
  int32_t motor_invert = s_lcdm_driver_params[0].value;

  if (motor_invert != s_lcdm_servo_prev_motor_invert)
  {
    s_lcdm_servo_dir_bad_count = 0U;
    s_lcdm_servo_dir_alarm = 0U;
    s_lcdm_servo_prev_feedback_adc = feedback_adc;
    s_lcdm_servo_prev_target_adc = target_adc;
    s_lcdm_servo_prev_motor_invert = motor_invert;
    return false;
  }

  if (s_lcdm_driver_params[1].value == 0)
  {
    s_lcdm_servo_dir_bad_count = 0U;
    s_lcdm_servo_dir_alarm = 0U;
    s_lcdm_servo_prev_feedback_adc = feedback_adc;
    s_lcdm_servo_prev_target_adc = target_adc;
    return false;
  }

  target_delta = (target_adc > s_lcdm_servo_prev_target_adc) ?
                 (uint16_t)(target_adc - s_lcdm_servo_prev_target_adc) :
                 (uint16_t)(s_lcdm_servo_prev_target_adc - target_adc);
  if ((s_lcdm_servo_prev_target_adc != 0xFFFFU) &&
      (target_delta > LCDM_SERVO_DIR_CHECK_MIN_ERROR))
  {
    s_lcdm_servo_dir_bad_count = 0U;
    s_lcdm_servo_dir_alarm = 0U;
  }

  if (s_lcdm_servo_prev_feedback_adc == 0xFFFFU)
  {
    s_lcdm_servo_prev_feedback_adc = feedback_adc;
    s_lcdm_servo_prev_target_adc = target_adc;
    return false;
  }

  feedback_delta = (int16_t)((int32_t)feedback_adc - (int32_t)s_lcdm_servo_prev_feedback_adc);
  s_lcdm_servo_prev_feedback_adc = feedback_adc;
  s_lcdm_servo_prev_target_adc = target_adc;

  if ((abs_error < LCDM_SERVO_DIR_CHECK_MIN_ERROR) || (abs_duty < LCDM_SERVO_DIR_CHECK_MIN_DUTY))
  {
    if (s_lcdm_servo_dir_bad_count > 0U)
    {
      s_lcdm_servo_dir_bad_count--;
    }
    return (s_lcdm_servo_dir_alarm != 0U);
  }

  if ((error_adc > 0) && (feedback_delta < 0))
  {
    moving_away = true;
    feedback_delta = (int16_t)-feedback_delta;
  }
  else if ((error_adc < 0) && (feedback_delta > 0))
  {
    moving_away = true;
  }

  if (moving_away && ((uint16_t)feedback_delta >= LCDM_SERVO_DIR_CHECK_MIN_STEP) &&
      ((uint16_t)feedback_delta <= LCDM_SERVO_DIR_CHECK_MAX_STEP))
  {
    if (s_lcdm_servo_dir_bad_count < LCDM_SERVO_DIR_CHECK_TRIP_COUNT)
    {
      s_lcdm_servo_dir_bad_count++;
    }
    if (s_lcdm_servo_dir_bad_count >= LCDM_SERVO_DIR_CHECK_TRIP_COUNT)
    {
      s_lcdm_servo_dir_alarm = 1U;
    }
  }
  else if (moving_away)
  {
    s_lcdm_servo_dir_bad_count = 0U;
  }
  else if (feedback_delta != 0)
  {
    s_lcdm_servo_dir_bad_count = 0U;
  }

  return (s_lcdm_servo_dir_alarm != 0U);
}

static int8_t lcdm_servo_duty_dir(int16_t duty)
{
  if (duty > 0)
  {
    return 1;
  }
  if (duty < 0)
  {
    return -1;
  }
  return 0;
}

static void lcdm_servo_clear_output_guard(void)
{
  s_lcdm_servo_output_dir = 0;
  s_lcdm_servo_reverse_dead_until_ms = 0U;
  s_lcdm_servo_reverse_dead_active = 0U;
}

static bool lcdm_servo_reverse_dead_active(uint32_t now_ms)
{
  if (s_lcdm_servo_reverse_dead_active == 0U)
  {
    return false;
  }
  if ((int32_t)(now_ms - s_lcdm_servo_reverse_dead_until_ms) < 0)
  {
    return true;
  }
  s_lcdm_servo_reverse_dead_active = 0U;
  s_lcdm_servo_output_dir = 0;
  return false;
}

static int16_t lcdm_servo_apply_slew(int16_t duty)
{
  int16_t slew = (int16_t)lcdm_param_u16(s_lcdm_reg_params,
                                         LCDM_REG_PARAM_SLEW,
                                         LCDM_SERVO_OUT_SLEW);

  if ((s_lcdm_servo_current_duty > 0) && (duty < 0))
  {
    int16_t next = (int16_t)(s_lcdm_servo_current_duty - slew);
    return (next > 0) ? next : 0;
  }
  if ((s_lcdm_servo_current_duty < 0) && (duty > 0))
  {
    int16_t next = (int16_t)(s_lcdm_servo_current_duty + slew);
    return (next < 0) ? next : 0;
  }
  if ((s_lcdm_servo_current_duty >= 0) && (duty > s_lcdm_servo_current_duty) &&
      ((duty - s_lcdm_servo_current_duty) > slew))
  {
    return (int16_t)(s_lcdm_servo_current_duty + slew);
  }
  if ((s_lcdm_servo_current_duty <= 0) && (duty < s_lcdm_servo_current_duty) &&
      ((s_lcdm_servo_current_duty - duty) > slew))
  {
    return (int16_t)(s_lcdm_servo_current_duty - slew);
  }
  return duty;
}

static void lcdm_servo_enter_hold(uint16_t control_pulse_us,
                                  uint16_t raw_adc,
                                  uint16_t target_adc,
                                  uint16_t feedback_adc,
                                  int16_t error_adc,
                                  uint8_t event)
{
  HBridge_Brake();
  s_lcdm_servo_vel_q4 = 0;
  s_lcdm_servo_current_velocity = 0;
  s_lcdm_servo_traj_pos_q4 = (int32_t)target_adc << 4;
  s_lcdm_servo_traj_vel_q4 = 0;
  s_lcdm_servo_ref_velocity = 0;
  s_lcdm_servo_current_duty = 0;
  lcdm_servo_clear_output_guard();
  s_lcdm_servo_current_state = LCDM_SERVO_STATE_HOLD;
  s_lcdm_servo_hold_active = 1U;
  s_lcdm_servo_v3_phase = LCDM_SERVO_V3_PHASE_HOLD;
  s_lcdm_servo_hold_anchor_adc = target_adc;
  s_lcdm_servo_move_dir = 0;
  s_lcdm_servo_motion_dir = 0;
  s_lcdm_servo_last_motion_error = 0xFFFFU;
  s_lcdm_servo_motion_settle_count = 0U;
  s_lcdm_servo_approach_brake_tail_count = 0U;
  s_lcdm_servo_last_control_feedback_adc = feedback_adc;
  lcdm_servo_curve_publish(control_pulse_us, raw_adc, target_adc, feedback_adc, error_adc, 0, event);
}

static int16_t __attribute__((noinline)) lcdm_servo_stop_publish(uint16_t control_pulse_us,
                                                                 uint16_t raw_adc,
                                                                 uint16_t target_adc,
                                                                 uint16_t feedback_adc,
                                                                 int16_t error_adc,
                                                                 uint8_t state,
                                                                 uint8_t event,
                                                                 bool brake)
{
  if (brake)
  {
    HBridge_Brake();
  }
  else
  {
    HBridge_Coast();
  }
  s_lcdm_servo_current_duty = 0;
  lcdm_servo_clear_output_guard();
  s_lcdm_servo_current_state = state;
  s_lcdm_servo_ref_velocity = 0;
  lcdm_servo_curve_publish(control_pulse_us, raw_adc, target_adc, feedback_adc, error_adc, 0, event);
  return 0;
}

static int16_t lcdm_servo_loop_update_fast(void)
{
#if SERVO_ENABLE_INTERNAL_STEP_TEST
  PWM_InputSample sample = {0};
#else
  PWM_InputSample sample = PWM_Input_GetSample();
#endif
  bool fresh;
  Board_PotPowerRefresh();
#if SERVO_ENABLE_INTERNAL_STEP_TEST
  lcdm_internal_step_test_sample(&sample);
#else
  if (lcdm_pwm_runtime_internal_enabled())
  {
    lcdm_runtime_pwm_test_sample(&sample);
  }
#endif
  fresh = lcdm_pwm_sample_is_fresh(&sample);
  uint16_t raw_adc;
  uint16_t feedback;
  uint16_t target_adc;
  uint16_t max_duty;
  int16_t duty = 0;
  int16_t error;
  int16_t velocity;
  LcdmPwmRangeState range_state = LCDM_PWM_RANGE_IN;
  uint16_t control_pulse_us;
  uint16_t target_delta = 0U;
  uint16_t pulse_delta = 0U;
  uint16_t frequency_hz = (uint16_t)s_lcdm_driver_params[2].value;
  int32_t raw_q4;
  int32_t v_ref_q4;
  int32_t vel_error_q4;
  int32_t cmd_q4;
  int32_t max_vel_q4 = lcdm_param_i32(s_lcdm_reg_params,
                                      LCDM_REG_PARAM_VMAX,
                                      LCDM_SERVO_TRAJ_MAX_VEL_Q4);
  int32_t vel_k_num = lcdm_param_i32(s_lcdm_reg_params,
                                     LCDM_REG_PARAM_KV,
                                     LCDM_SERVO_VEL_K_NUM);
  uint16_t moving_ff_duty = lcdm_param_u16(s_lcdm_reg_params,
                                           LCDM_REG_PARAM_FF,
                                           LCDM_SERVO_MOVING_FF_DUTY);
  uint16_t brake_vel_gain = lcdm_param_u16(s_lcdm_step_params,
                                           LCDM_BRAKE_PARAM_VEL_GAIN,
                                           LCDM_SERVO_V3_BRAKE_VEL_GAIN);
  uint16_t brake_min_band = lcdm_param_u16(s_lcdm_step_params,
                                           LCDM_BRAKE_PARAM_MIN_BAND,
                                           LCDM_SERVO_V3_MIN_BRAKE_BAND);
  uint16_t brake_max_band = lcdm_param_u16(s_lcdm_step_params,
                                           LCDM_BRAKE_PARAM_MAX_BAND,
                                           LCDM_SERVO_V3_BRAKE_MAX_BAND);
  uint16_t brake_min_vel_q4 = lcdm_param_u16(s_lcdm_step_params,
                                             LCDM_BRAKE_PARAM_MIN_VEL,
                                             LCDM_SERVO_V3_BRAKE_MIN_VEL_Q4);
  uint16_t brake_high_vel_q4 = lcdm_param_u16(s_lcdm_step_params,
                                              LCDM_BRAKE_PARAM_HOLD_VEL,
                                              LCDM_SERVO_V3_BRAKE_HOLD_VEL_Q4);
  uint16_t motion_hold_band = lcdm_param_u16(s_lcdm_template_params,
                                             LCDM_HOLD_PARAM_BAND,
                                             LCDM_SERVO_V3_MOTION_HOLD_BAND);
  uint16_t motion_hold_vel_q4 = lcdm_param_u16(s_lcdm_template_params,
                                               LCDM_HOLD_PARAM_VEL,
                                               LCDM_SERVO_V3_MOTION_HOLD_VEL_Q4);
  uint16_t hold_release_band = lcdm_param_u16(s_lcdm_template_params,
                                              LCDM_HOLD_PARAM_RELEASE,
                                              LCDM_SERVO_HOLD_RELEASE_BAND);
  uint16_t lock_max_duty = lcdm_param_u16(s_lcdm_template_params,
                                          LCDM_HOLD_PARAM_LOCK_MAX,
                                          LCDM_SERVO_V3_LOCK_MAX_DUTY);
  uint16_t lock_min_duty = lcdm_param_u16(s_lcdm_template_params,
                                          LCDM_HOLD_PARAM_LOCK_MIN,
                                          LCDM_SERVO_LOCK_LIGHT_BASE_DUTY);
  uint16_t abs_error;
  uint16_t abs_velocity_q4;
  bool target_changed = false;
  bool target_motion_changed = false;
  uint8_t drive_event = LCDM_SERVO_V3_EVENT_DRIVE;
  uint16_t loop_max_duty;
  int32_t duty_calc;
  bool static_lock_mode = false;
  uint16_t approach_band;
  bool moving_toward_target;
  bool crossed_target;
  bool command_step;
  bool static_lock_candidate;
  uint16_t v7_settle_band;
  uint16_t v7_static_lock_band;
  uint16_t v7_stop_vel_limit_q4;
  bool cross_moving_away;
  bool v7_need_prebrake;

  if (s_lcdm_servo_v3_phase == LCDM_SERVO_V3_PHASE_LOCK)
  {
    raw_adc = ADC_Feedback_ReadFastRaw();
  }
  else
  {
    raw_adc = ADC_Feedback_ReadRaw();
  }

  if (frequency_hz != s_lcdm_last_hbridge_frequency_hz)
  {
    HBridge_SetFrequency(frequency_hz);
    s_lcdm_last_hbridge_frequency_hz = frequency_hz;
  }

  if (!lcdm_servo_adc_is_valid(raw_adc))
  {
    HBridge_Coast();
    s_lcdm_servo_current_duty = 0;
    lcdm_servo_clear_output_guard();
    s_lcdm_servo_current_velocity = 0;
    s_lcdm_servo_ref_velocity = 0;
    s_lcdm_servo_hold_active = 0U;
    s_lcdm_servo_dir_bad_count = 0U;
    s_lcdm_servo_dir_alarm = 0U;
    s_lcdm_servo_filter_seeded = 0U;
    s_lcdm_servo_adc_jump_count = 0U;
    s_lcdm_servo_prev_feedback_adc = 0xFFFFU;
    s_lcdm_servo_velocity_feedback_adc = 0xFFFFU;
    s_lcdm_servo_prev_target_adc = 0xFFFFU;
    s_lcdm_servo_hold_deviation_count = 0U;
    s_lcdm_servo_last_motion_error = 0xFFFFU;
    s_lcdm_servo_motion_start_error = 0U;
    s_lcdm_servo_motion_settle_count = 0U;
    s_lcdm_servo_approach_brake_tail_count = 0U;
    s_lcdm_servo_v3_phase = LCDM_SERVO_V3_PHASE_IDLE;
    s_lcdm_servo_ab_seeded = 0U;
    if (s_lcdm_servo_adc_lost_count < LCDM_SERVO_ADC_LOST_TRIP_COUNT)
    {
      s_lcdm_servo_adc_lost_count++;
    }
    s_lcdm_servo_current_state = (s_lcdm_servo_adc_lost_count >= LCDM_SERVO_ADC_LOST_TRIP_COUNT) ?
                                 LCDM_SERVO_STATE_ADC_LOST :
                                 LCDM_SERVO_STATE_HOLD;
    return 0;
  }
  s_lcdm_servo_adc_lost_count = 0U;

  if (max_vel_q4 < 16)
  {
    max_vel_q4 = 16;
  }
  if (vel_k_num < 1)
  {
    vel_k_num = 1;
  }

  raw_q4 = (int32_t)raw_adc << 4;
  if (s_lcdm_servo_ab_seeded == 0U)
  {
    s_lcdm_servo_pos_q4 = raw_q4;
    s_lcdm_servo_vel_q4 = 0;
    s_lcdm_servo_traj_pos_q4 = raw_q4;
    s_lcdm_servo_traj_vel_q4 = 0;
    s_lcdm_servo_cmd_target_adc = raw_adc;
    s_lcdm_servo_ab_seeded = 1U;
  }
  else
  {
    int32_t predicted_q4 = s_lcdm_servo_pos_q4 + s_lcdm_servo_vel_q4;
    int32_t residual_q4 = raw_q4 - predicted_q4;
    int32_t abs_residual_q4 = (residual_q4 < 0) ? -residual_q4 : residual_q4;
    bool fast_observer = (s_lcdm_servo_current_duty != 0) ||
                         (s_lcdm_servo_v3_phase == LCDM_SERVO_V3_PHASE_ACCEL) ||
                         (s_lcdm_servo_v3_phase == LCDM_SERVO_V3_PHASE_TRACK) ||
                         (s_lcdm_servo_v3_phase == LCDM_SERVO_V3_PHASE_BRAKE) ||
                         (s_lcdm_servo_v3_phase == LCDM_SERVO_V3_PHASE_RETURN) ||
                         (abs_residual_q4 > LCDM_SERVO_OBS_FAST_RESIDUAL_Q4);
    int32_t pos_div = fast_observer ? LCDM_SERVO_OBS_POS_DIV_FAST : LCDM_SERVO_OBS_POS_DIV_SLOW;
    int32_t vel_div = fast_observer ? LCDM_SERVO_OBS_VEL_DIV_FAST : LCDM_SERVO_OBS_VEL_DIV_SLOW;
    s_lcdm_servo_pos_q4 = predicted_q4 + (residual_q4 / pos_div);
    s_lcdm_servo_vel_q4 += (residual_q4 / vel_div);
    if (s_lcdm_servo_vel_q4 > 512)
    {
      s_lcdm_servo_vel_q4 = 512;
    }
    else if (s_lcdm_servo_vel_q4 < -512)
    {
      s_lcdm_servo_vel_q4 = -512;
    }
  }
  feedback = (uint16_t)(s_lcdm_servo_pos_q4 >> 4);
  velocity = (int16_t)(s_lcdm_servo_vel_q4 / 16);
  s_lcdm_servo_current_velocity = velocity;
  s_lcdm_servo_filtered_feedback_adc = feedback;
  s_lcdm_servo_filter_seeded = 1U;
  lcdm_servo_adc_window_add(feedback);

  if (!fresh)
  {
    HBridge_Coast();
    s_lcdm_servo_current_duty = 0;
    lcdm_servo_clear_output_guard();
    s_lcdm_servo_current_velocity = 0;
    s_lcdm_servo_ref_velocity = 0;
    s_lcdm_servo_current_state = (sample.edge_count == 0UL) ? LCDM_SERVO_STATE_NO_PWM : LCDM_SERVO_STATE_BAD;
    s_lcdm_servo_current_range = LCDM_PWM_RANGE_IN;
    s_lcdm_servo_hold_deviation_count = 0U;
    s_lcdm_servo_last_motion_error = 0xFFFFU;
    s_lcdm_servo_motion_start_error = 0U;
    s_lcdm_servo_motion_settle_count = 0U;
    s_lcdm_servo_approach_brake_tail_count = 0U;
    s_lcdm_servo_v3_phase = LCDM_SERVO_V3_PHASE_IDLE;
    s_lcdm_servo_traj_pos_q4 = s_lcdm_servo_pos_q4;
    s_lcdm_servo_traj_vel_q4 = 0;
    return 0;
  }

  range_state = lcdm_pwm_range_state(sample.pulse_us);
  s_lcdm_servo_current_range = range_state;
  if ((range_state != LCDM_PWM_RANGE_IN) &&
      (s_lcdm_startup_params[LCDM_STARTUP_PARAM_RANGE_MODE].value == LCDM_SERVO_RANGE_RELEASE))
  {
    HBridge_Coast();
    s_lcdm_servo_current_duty = 0;
    lcdm_servo_clear_output_guard();
    s_lcdm_servo_current_velocity = 0;
    s_lcdm_servo_ref_velocity = 0;
    s_lcdm_servo_current_state = LCDM_SERVO_STATE_RANGE_OFF;
    s_lcdm_servo_hold_active = 0U;
    s_lcdm_servo_dir_bad_count = 0U;
    s_lcdm_servo_hold_deviation_count = 0U;
    s_lcdm_servo_last_motion_error = 0xFFFFU;
    s_lcdm_servo_motion_start_error = 0U;
    s_lcdm_servo_motion_settle_count = 0U;
    s_lcdm_servo_approach_brake_tail_count = 0U;
    s_lcdm_servo_v3_phase = LCDM_SERVO_V3_PHASE_IDLE;
    s_lcdm_servo_traj_pos_q4 = s_lcdm_servo_pos_q4;
    s_lcdm_servo_traj_vel_q4 = 0;
    return 0;
  }

  control_pulse_us = lcdm_servo_update_latched_pulse(sample.pulse_us, sample.pulse_count);
  target_adc = lcdm_pwm_to_target_adc(control_pulse_us);
  if (s_lcdm_servo_last_control_pulse_us != 0xFFFFU)
  {
    pulse_delta = (control_pulse_us > s_lcdm_servo_last_control_pulse_us) ?
                  (uint16_t)(control_pulse_us - s_lcdm_servo_last_control_pulse_us) :
                  (uint16_t)(s_lcdm_servo_last_control_pulse_us - control_pulse_us);
  }
  if (s_lcdm_servo_last_target_adc != 0xFFFFU)
  {
    target_delta = (target_adc > s_lcdm_servo_last_target_adc) ?
                   (uint16_t)(target_adc - s_lcdm_servo_last_target_adc) :
                   (uint16_t)(s_lcdm_servo_last_target_adc - target_adc);
    target_changed = (pulse_delta != 0U) || (target_delta > LCDM_SERVO_CMD_HOLD_EXIT_EXTRA);
    target_motion_changed = (pulse_delta != 0U) || (target_delta > LCDM_SERVO_CMD_MOTION_EXIT_EXTRA);
  }
  else
  {
    target_changed = true;
    target_motion_changed = true;
  }

  if (target_motion_changed)
  {
    s_lcdm_servo_cmd_sensitive = 80U;
  }
  else if (s_lcdm_servo_cmd_sensitive > 0U)
  {
    s_lcdm_servo_cmd_sensitive--;
  }
  command_step = target_changed;
  if (target_changed)
  {
    s_lcdm_servo_cmd_target_adc = target_adc;
  }
  if (command_step)
  {
    if (s_lcdm_servo_last_target_adc != 0xFFFFU)
    {
      lcdm_servo_curve_log_trigger(HAL_GetTick());
    }
    uint16_t motion_error = (target_adc > feedback) ?
                            (uint16_t)(target_adc - feedback) :
                            (uint16_t)(feedback - target_adc);
    s_lcdm_servo_hold_active = 0U;
    s_lcdm_servo_v3_phase = LCDM_SERVO_V3_PHASE_ACCEL;
    s_lcdm_servo_move_dir = (target_adc > feedback) ? 1 : ((target_adc < feedback) ? -1 : 0);
    s_lcdm_servo_motion_dir = s_lcdm_servo_move_dir;
    s_lcdm_servo_overshoot_adc = 0U;
    s_lcdm_servo_hold_deviation_count = 0U;
    s_lcdm_servo_last_motion_error = motion_error;
    s_lcdm_servo_motion_start_error = motion_error;
    s_lcdm_servo_motion_settle_count = 0U;
    s_lcdm_servo_approach_brake_tail_count = 0U;
    s_lcdm_servo_traj_pos_q4 = s_lcdm_servo_pos_q4;
    s_lcdm_servo_traj_vel_q4 = 0;
    s_servo_curve_static_lock_logged = 0U;
  }
  s_lcdm_servo_last_target_adc = target_adc;
  s_lcdm_servo_last_control_pulse_us = control_pulse_us;

  error = (int16_t)((int32_t)s_lcdm_servo_cmd_target_adc - (int32_t)feedback);
  abs_error = (error < 0) ? (uint16_t)(-error) : (uint16_t)error;
  abs_velocity_q4 = (s_lcdm_servo_vel_q4 < 0) ?
                    (uint16_t)(-s_lcdm_servo_vel_q4) :
                    (uint16_t)s_lcdm_servo_vel_q4;
  if ((s_lcdm_servo_move_dir > 0) && (feedback > s_lcdm_servo_cmd_target_adc))
  {
    uint16_t over = (uint16_t)(feedback - s_lcdm_servo_cmd_target_adc);
    if (over > s_lcdm_servo_overshoot_adc)
    {
      s_lcdm_servo_overshoot_adc = over;
    }
  }
  else if ((s_lcdm_servo_move_dir < 0) && (feedback < s_lcdm_servo_cmd_target_adc))
  {
    uint16_t over = (uint16_t)(s_lcdm_servo_cmd_target_adc - feedback);
    if (over > s_lcdm_servo_overshoot_adc)
    {
      s_lcdm_servo_overshoot_adc = over;
    }
  }

  max_duty = (uint16_t)s_lcdm_driver_params[3].value;
  if (max_duty > HBRIDGE_DUTY_MAX)
	  {
	    max_duty = HBRIDGE_DUTY_MAX;
	  }
  loop_max_duty = max_duty;

  cmd_q4 = (int32_t)s_lcdm_servo_cmd_target_adc << 4;
  s_lcdm_servo_traj_pos_q4 = cmd_q4;
  s_lcdm_servo_traj_vel_q4 = 0;

  if (s_lcdm_servo_motion_dir == 0)
  {
    s_lcdm_servo_motion_dir = (error > 0) ? 1 : ((error < 0) ? -1 : 0);
  }
  crossed_target = ((s_lcdm_servo_motion_dir > 0) && (error < 0)) ||
                   ((s_lcdm_servo_motion_dir < 0) && (error > 0));
  moving_toward_target = ((error > 0) && (s_lcdm_servo_vel_q4 > (int32_t)brake_min_vel_q4)) ||
                          ((error < 0) && (s_lcdm_servo_vel_q4 < -(int32_t)brake_min_vel_q4));
  approach_band = (uint16_t)(brake_min_band + (abs_velocity_q4 * brake_vel_gain) / 3U);
  if (approach_band > brake_max_band)
  {
    approach_band = brake_max_band;
  }
  v7_settle_band = (uint16_t)(hold_release_band + motion_hold_band);
  v7_static_lock_band = (uint16_t)(hold_release_band + (motion_hold_band * 3U));
  cross_moving_away = ((error > 0) && (s_lcdm_servo_vel_q4 < -(int32_t)motion_hold_vel_q4)) ||
                      ((error < 0) && (s_lcdm_servo_vel_q4 > (int32_t)motion_hold_vel_q4));
  if (abs_error >= approach_band)
  {
    v7_stop_vel_limit_q4 = (uint16_t)max_vel_q4;
  }
  else
  {
    v7_stop_vel_limit_q4 = (uint16_t)(((uint32_t)abs_error * (uint32_t)max_vel_q4) /
                                      (uint32_t)approach_band);
    if (v7_stop_vel_limit_q4 < motion_hold_vel_q4)
    {
      v7_stop_vel_limit_q4 = motion_hold_vel_q4;
    }
  }
  v7_need_prebrake = moving_toward_target &&
                     (abs_velocity_q4 > (uint16_t)(v7_stop_vel_limit_q4 + brake_high_vel_q4));
  static_lock_candidate = (!command_step) &&
                          ((s_lcdm_servo_v3_phase == LCDM_SERVO_V3_PHASE_HOLD) ||
                           (s_lcdm_servo_v3_phase == LCDM_SERVO_V3_PHASE_LOCK)) &&
                          ((abs_error > v7_static_lock_band) ||
                           (abs_velocity_q4 > (uint16_t)(motion_hold_vel_q4 + 48U)));
  static_lock_mode = static_lock_candidate;
  if (static_lock_candidate && (s_lcdm_servo_v3_phase != LCDM_SERVO_V3_PHASE_LOCK))
  {
    if (s_lcdm_servo_hold_deviation_count < 2U)
    {
      s_lcdm_servo_hold_deviation_count++;
    }
    static_lock_mode = (s_lcdm_servo_hold_deviation_count >= 2U);
  }
  else if (!static_lock_candidate)
  {
    s_lcdm_servo_hold_deviation_count = 0U;
  }

  duty_calc = 0;
  v_ref_q4 = 0;
  if (static_lock_mode)
  {
    if (s_servo_curve_static_lock_logged == 0U)
    {
      if (s_servo_curve_log_active == 0U)
      {
        lcdm_servo_curve_log_trigger(HAL_GetTick());
      }
      s_servo_curve_static_lock_logged = 1U;
    }
    s_lcdm_servo_v3_phase = LCDM_SERVO_V3_PHASE_LOCK;
    s_lcdm_servo_hold_active = 0U;
    loop_max_duty = (loop_max_duty < lock_max_duty) ? loop_max_duty : lock_max_duty;
  }
  else if (abs_error <= hold_release_band)
  {
    s_servo_curve_static_lock_logged = 0U;
  }

  if ((!command_step) &&
      (abs_error <= hold_release_band) &&
      (abs_velocity_q4 <= motion_hold_vel_q4))
  {
    lcdm_servo_enter_hold(control_pulse_us, raw_adc, s_lcdm_servo_cmd_target_adc, feedback, error, 2U);
    return 0;
  }

  if ((!command_step) &&
      ((s_lcdm_servo_v3_phase == LCDM_SERVO_V3_PHASE_HOLD) ||
       (s_lcdm_servo_v3_phase == LCDM_SERVO_V3_PHASE_LOCK)) &&
      (!static_lock_mode) &&
      (abs_error <= (uint16_t)(v7_settle_band + (motion_hold_band / 2U))) &&
      (abs_velocity_q4 <= (uint16_t)(brake_high_vel_q4 + 32U)))
  {
    lcdm_servo_enter_hold(control_pulse_us, raw_adc, s_lcdm_servo_cmd_target_adc, feedback, error, 2U);
    return 0;
  }

  if ((!command_step) &&
      (crossed_target || static_lock_candidate) &&
      (!static_lock_mode) &&
      (abs_error <= v7_settle_band) &&
      (abs_velocity_q4 <= (uint16_t)(motion_hold_vel_q4 + 16U)))
  {
    lcdm_servo_enter_hold(control_pulse_us, raw_adc, s_lcdm_servo_cmd_target_adc, feedback, error, 2U);
    return 0;
  }

  if (static_lock_mode)
  {
    uint16_t lock_abs = (uint16_t)(lock_min_duty + (abs_error * LCDM_SERVO_V6_LOCK_GAIN));
    if (lock_abs > lock_max_duty)
    {
      lock_abs = lock_max_duty;
    }
    duty_calc = (error >= 0) ? (int32_t)lock_abs : -(int32_t)lock_abs;
    drive_event = LCDM_SERVO_V3_EVENT_LOCK;
    s_lcdm_servo_v3_phase = LCDM_SERVO_V3_PHASE_LOCK;
  }
  else if (crossed_target && !SERVO_ENABLE_INTERNAL_STEP_TEST)
  {
    uint16_t reverse_abs;
    if ((!cross_moving_away) &&
        (abs_error <= v7_static_lock_band) &&
        (abs_velocity_q4 <= (uint16_t)(brake_high_vel_q4 + 16U)))
    {
      lcdm_servo_enter_hold(control_pulse_us, raw_adc, s_lcdm_servo_cmd_target_adc, feedback, error, 2U);
      return 0;
    }
    reverse_abs = (uint16_t)((abs_error * 3U) + (abs_velocity_q4 / 4U));
    if ((abs_error > v7_static_lock_band) &&
        (reverse_abs < LCDM_SERVO_V3_RETURN_MIN_DUTY))
    {
      reverse_abs = LCDM_SERVO_V3_RETURN_MIN_DUTY;
    }
    if (reverse_abs > LCDM_SERVO_V3_RETURN_MAX_DUTY)
    {
      reverse_abs = LCDM_SERVO_V3_RETURN_MAX_DUTY;
    }
    duty_calc = (error >= 0) ? (int32_t)reverse_abs : -(int32_t)reverse_abs;
    drive_event = LCDM_SERVO_V3_EVENT_CROSS_BRAKE;
    s_lcdm_servo_v3_phase = LCDM_SERVO_V3_PHASE_BRAKE;
    s_lcdm_servo_motion_settle_count = 0U;
  }
  else if (v7_need_prebrake)
  {
    uint16_t reverse_abs = (uint16_t)(abs_velocity_q4 + (abs_error / 8U));
    if ((abs_error <= brake_min_band) &&
        (abs_velocity_q4 > (uint16_t)(motion_hold_vel_q4 + 16U)))
    {
      reverse_abs = (uint16_t)(reverse_abs + (abs_velocity_q4 / 2U) + (moving_ff_duty / 2U));
    }
    if (reverse_abs < LCDM_SERVO_V3_RETURN_MIN_DUTY)
    {
      reverse_abs = LCDM_SERVO_V3_RETURN_MIN_DUTY;
    }
    if (reverse_abs > LCDM_SERVO_V3_RETURN_MAX_DUTY)
    {
      reverse_abs = LCDM_SERVO_V3_RETURN_MAX_DUTY;
    }
    duty_calc = (s_lcdm_servo_vel_q4 > 0) ? -(int32_t)reverse_abs : (int32_t)reverse_abs;
    drive_event = LCDM_SERVO_V3_EVENT_BRAKE;
    s_lcdm_servo_v3_phase = LCDM_SERVO_V3_PHASE_BRAKE;
    s_lcdm_servo_motion_settle_count = 0U;
  }
  else
  {
    v_ref_q4 = (error >= 0) ? (int32_t)v7_stop_vel_limit_q4 : -(int32_t)v7_stop_vel_limit_q4;
    vel_error_q4 = v_ref_q4 - s_lcdm_servo_vel_q4;
    duty_calc = vel_error_q4 * vel_k_num;
    duty_calc += (int32_t)error / (int32_t)LCDM_SERVO_V7_DRIVE_ERR_DIV;
    duty_calc -= ((int32_t)s_lcdm_servo_vel_q4 * (int32_t)brake_vel_gain);
    if (v_ref_q4 > 24)
    {
      duty_calc += moving_ff_duty;
    }
    else if (v_ref_q4 < -24)
    {
      duty_calc -= moving_ff_duty;
    }
    if ((!v7_need_prebrake) && (!crossed_target) && (abs_error > approach_band))
    {
      int32_t floor = (int32_t)lock_min_duty + (int32_t)moving_ff_duty +
                      (int32_t)((abs_error - approach_band) >> 1);
      if (floor > (int32_t)loop_max_duty)
      {
        floor = (int32_t)loop_max_duty;
      }
      if ((error > 0) && (duty_calc < floor))
      {
        duty_calc = floor;
      }
      else if ((error < 0) && (duty_calc > -floor))
      {
        duty_calc = -floor;
      }
    }
    if ((!moving_toward_target) &&
        (abs_error > motion_hold_band) &&
        (abs_velocity_q4 <= brake_min_vel_q4))
    {
      if (s_lcdm_servo_motion_settle_count < LCDM_SERVO_V7_LOAD_CONFIRM_COUNT)
      {
        s_lcdm_servo_motion_settle_count++;
      }
      if (s_lcdm_servo_motion_settle_count >= LCDM_SERVO_V7_LOAD_CONFIRM_COUNT)
      {
        uint16_t push_abs = (uint16_t)(lock_min_duty + (abs_error * LCDM_SERVO_V6_DRIVE_ERR_GAIN));
        if (push_abs > loop_max_duty)
        {
          push_abs = loop_max_duty;
        }
        duty_calc = (error >= 0) ? (int32_t)push_abs : -(int32_t)push_abs;
        drive_event = LCDM_SERVO_V3_EVENT_LOCK;
        s_lcdm_servo_v3_phase = LCDM_SERVO_V3_PHASE_LOCK;
      }
      else
      {
        drive_event = LCDM_SERVO_V3_EVENT_DRIVE;
        s_lcdm_servo_v3_phase = LCDM_SERVO_V3_PHASE_ACCEL;
      }
    }
    else
    {
      s_lcdm_servo_motion_settle_count = 0U;
      drive_event = (abs_error <= approach_band) ?
                    LCDM_SERVO_V3_EVENT_DECEL :
                    LCDM_SERVO_V3_EVENT_DRIVE;
      s_lcdm_servo_v3_phase = (drive_event == LCDM_SERVO_V3_EVENT_DECEL) ?
                              LCDM_SERVO_V3_PHASE_TRACK :
                              LCDM_SERVO_V3_PHASE_ACCEL;
    }
    s_lcdm_servo_last_motion_error = abs_error;
  }

  if (static_lock_mode && (s_lcdm_servo_v3_phase == LCDM_SERVO_V3_PHASE_LOCK))
  {
    s_lcdm_servo_ref_velocity = 0;
  }
  else
  {
    s_lcdm_servo_ref_velocity = (int16_t)(v_ref_q4 / 16);
  }

  if (duty_calc > (int32_t)loop_max_duty)
  {
    duty_calc = (int32_t)loop_max_duty;
  }
  else if (duty_calc < -(int32_t)loop_max_duty)
  {
    duty_calc = -(int32_t)loop_max_duty;
  }

  duty = (int16_t)duty_calc;

  s_lcdm_servo_move_dir = (error > 0) ? 1 : ((error < 0) ? -1 : 0);
  s_lcdm_servo_hold_active = 0U;

  if (s_lcdm_driver_params[0].value != 0)
  {
    duty = (int16_t)-duty;
  }

  {
    uint32_t now_ms = HAL_GetTick();
    int8_t requested_dir = lcdm_servo_duty_dir(duty);
    uint16_t requested_abs_duty = (duty < 0) ? (uint16_t)(-duty) : (uint16_t)duty;

    if (lcdm_servo_reverse_dead_active(now_ms))
    {
      HBridge_Coast();
      s_lcdm_servo_current_duty = 0;
      s_lcdm_servo_ref_velocity = 0;
      s_lcdm_servo_current_state = LCDM_SERVO_STATE_BRAKE;
      lcdm_servo_curve_publish(control_pulse_us, raw_adc, s_lcdm_servo_cmd_target_adc,
                               feedback, error, 0, 3U);
      return 0;
    }

    if ((s_lcdm_servo_output_dir != 0) &&
        (requested_dir != 0) &&
        (requested_dir != s_lcdm_servo_output_dir))
    {
      HBridge_Coast();
      s_lcdm_servo_current_duty = 0;
      s_lcdm_servo_ref_velocity = 0;
      s_lcdm_servo_current_state = LCDM_SERVO_STATE_BRAKE;
      if (requested_abs_duty >= LCDM_SERVO_REVERSE_MIN_DUTY)
      {
        s_lcdm_servo_reverse_dead_until_ms = now_ms + LCDM_SERVO_REVERSE_DEAD_MS;
        s_lcdm_servo_reverse_dead_active = 1U;
      }
      else
      {
        lcdm_servo_clear_output_guard();
      }
      lcdm_servo_curve_publish(control_pulse_us, raw_adc, s_lcdm_servo_cmd_target_adc,
                               feedback, error, 0, 3U);
      return 0;
    }
  }

  duty = lcdm_servo_apply_slew(duty);
  if (duty == 0)
  {
    return lcdm_servo_stop_publish(control_pulse_us, raw_adc, s_lcdm_servo_cmd_target_adc,
                                   feedback, error, LCDM_SERVO_STATE_BRAKE,
                                   (drive_event == LCDM_SERVO_V3_EVENT_BRAKE) ? drive_event : 3U,
                                   true);
  }

  if (lcdm_servo_reverse_alarm_update(s_lcdm_servo_cmd_target_adc, feedback, error, duty))
  {
    return lcdm_servo_stop_publish(control_pulse_us, raw_adc, s_lcdm_servo_cmd_target_adc,
                                   feedback, error, LCDM_SERVO_STATE_DIR_ALARM, 8U, false);
  }

  HBridge_SetSignedDuty(duty);
  s_lcdm_servo_current_duty = duty;
  s_lcdm_servo_output_dir = lcdm_servo_duty_dir(duty);
  s_lcdm_servo_reverse_dead_active = 0U;
  s_lcdm_servo_current_state = LCDM_SERVO_STATE_DRIVE;
  lcdm_servo_curve_publish(control_pulse_us, raw_adc, s_lcdm_servo_cmd_target_adc, feedback, error, duty, drive_event);
  return duty;
}

static void lcdm_draw_p50(uint16_t step)
{
  (void)step;

  lcdm_page_base("P50" LCDM_SERVO_FW_TAG, lcdm_input_mode_text());
  lcdm_row(0U, "P", "-- us", LCDM_COLOR_BLUE);
  lcdm_row(1U, "T", "--", LCDM_COLOR_BLUE);
  lcdm_row(2U, "A", "--", LCDM_COLOR_BLUE);
  lcdm_row(3U, "E", "--", LCDM_COLOR_RED);
  lcdm_row(4U, "H", "W", LCDM_COLOR_RED);
  lcdm_footer("M", "IN", "4");
  lcdm_reset_pwm_live_cache();
}

static void lcdm_wait_1ms_with_heartbeat(void)
{
  static uint16_t phase;

  phase++;
  if (phase >= 500U)
  {
    phase = 0U;
  }

  if (phase < 35U)
  {
    GPIOA->BRR = GPIO_PIN_5;
  }
  else
  {
    GPIOA->BSRR = GPIO_PIN_5;
  }
  wait_systick_ticks(SystemCoreClock / 1000U);
}

static bool lcdm_read_touch(LcdmRuntimeTouch *touch, uint16_t wait_ms)
{
  for (uint16_t ms = 0U; ms < wait_ms; ms++)
  {
    uint8_t byte;
    while (pb2_tjc_read_byte(&byte, 1U))
    {
      if (s_lcdm_touch_index == 0U)
      {
        if (byte != 0x67U)
        {
          continue;
        }
        s_lcdm_touch_frame[s_lcdm_touch_index++] = byte;
        continue;
      }

      s_lcdm_touch_frame[s_lcdm_touch_index++] = byte;
      if (s_lcdm_touch_index >= sizeof(s_lcdm_touch_frame))
      {
        s_lcdm_touch_index = 0U;
        if ((s_lcdm_touch_frame[0] == 0x67U) &&
            (s_lcdm_touch_frame[6] == 0xFFU) &&
            (s_lcdm_touch_frame[7] == 0xFFU) &&
            (s_lcdm_touch_frame[8] == 0xFFU))
        {
          touch->x = (uint16_t)(((uint16_t)s_lcdm_touch_frame[1] << 8) | s_lcdm_touch_frame[2]);
          touch->y = (uint16_t)(((uint16_t)s_lcdm_touch_frame[3] << 8) | s_lcdm_touch_frame[4]);
          touch->event = s_lcdm_touch_frame[5];
          return true;
        }
      }
    }
    lcdm_wait_1ms_with_heartbeat();
  }

  return false;
}

static int8_t lcdm_menu_row_from_xy(uint16_t x, uint16_t y, uint8_t max_items)
{
  uint16_t adjusted_y;
  int16_t grid_row;
  uint8_t grid_col;
  int16_t item;

  if (((uint16_t)(y + LCDM_TOUCH_Y_SLOP) < LCDM_MENU_GRID_Y) || (y >= LCDM_FOOTER_TOUCH_Y))
  {
    return -1;
  }

  adjusted_y = y;
  if (adjusted_y < LCDM_MENU_GRID_Y)
  {
    adjusted_y = LCDM_MENU_GRID_Y;
  }

  grid_row = (int16_t)((adjusted_y - LCDM_MENU_GRID_Y) / LCDM_MENU_ROW_STEP);
  if ((grid_row < 0) || (grid_row >= (int16_t)LCDM_MENU_ROWS))
  {
    return -1;
  }

  grid_col = (x < (LCDM_W / 2U)) ? 0U : 1U;
  item = (int16_t)(((uint16_t)grid_col * LCDM_MENU_ROWS) + (uint16_t)grid_row);
  if ((item < 0) || (item >= (int16_t)max_items))
  {
    return -1;
  }
  return (int8_t)item;
}

static int8_t lcdm_row_from_y(uint16_t y, uint8_t max_rows)
{
  uint16_t adjusted_y;
  int16_t row;

  if (((uint16_t)(y + LCDM_TOUCH_Y_SLOP) < LCDM_ROW_Y) || (y >= LCDM_FOOTER_TOUCH_Y))
  {
    return -1;
  }

  adjusted_y = y;
  if (adjusted_y < LCDM_ROW_Y)
  {
    adjusted_y = LCDM_ROW_Y;
  }

  row = (int16_t)((adjusted_y - LCDM_ROW_Y) / LCDM_ROW_STEP);
  if ((row < 0) || (row >= (int16_t)max_rows))
  {
    return -1;
  }
  return (int8_t)row;
}

static void lcdm_update_pwm_live_values(void)
{
  char text[32];
#if SERVO_ENABLE_INTERNAL_STEP_TEST
  PWM_InputSample sample = {0};
  lcdm_internal_step_test_sample(&sample);
#else
  PWM_InputSample sample = PWM_Input_GetSample();
  bool internal_source = lcdm_pwm_runtime_internal_enabled();
  if (internal_source)
  {
    lcdm_runtime_pwm_test_sample(&sample);
  }
#endif
  bool fresh = lcdm_pwm_sample_is_fresh(&sample);
  bool has_edge = (sample.edge_count != 0UL);
  LcdmPwmRangeState range_state = fresh ? lcdm_pwm_range_state(sample.pulse_us) : LCDM_PWM_RANGE_IN;
  bool range_lock = (s_lcdm_startup_params[LCDM_STARTUP_PARAM_RANGE_MODE].value == LCDM_SERVO_RANGE_LOCK);
  bool show_target = fresh && ((range_state == LCDM_PWM_RANGE_IN) || range_lock);
  uint16_t pwm_window =
#if SERVO_ENABLE_INTERNAL_STEP_TEST
    0U;
#else
    internal_source ? 0U : PWM_Input_TakeRawWindowUs();
#endif
  uint16_t feedback_adc = (s_lcdm_servo_filter_seeded != 0U) ?
                          s_lcdm_servo_filtered_feedback_adc :
                          ADC_Feedback_GetFiltered();
  uint16_t raw_adc = ADC_Feedback_GetRaw();
  uint16_t adc_window = lcdm_servo_adc_window_take();
  uint16_t target_pulse_us = (s_lcdm_servo_last_control_pulse_us != 0xFFFFU) ?
                             s_lcdm_servo_last_control_pulse_us :
                             sample.pulse_us;
  uint16_t target_mv = show_target ? lcdm_pwm_to_target_mv(target_pulse_us) : 0U;
  uint16_t target_adc = show_target ? lcdm_pwm_to_target_adc(target_pulse_us) : 0U;
  int16_t error_adc = show_target ? (int16_t)((int32_t)target_adc - (int32_t)feedback_adc) : 0;
  int16_t duty = s_lcdm_servo_current_duty;
  int16_t adc_velocity = s_lcdm_servo_current_velocity;
  uint8_t event = g_servo_curve_frame.event;
  int8_t direction = (duty > 0) ? 1 : ((duty < 0) ? -1 : 0);
  uint16_t abs_duty = (duty < 0) ? (uint16_t)(-duty) : (uint16_t)duty;
  uint8_t state_code;

  if (has_edge &&
      ((target_pulse_us != s_lcdm_last_pwm_control_us) ||
       (sample.raw_pulse_us != s_lcdm_last_pwm_pulse_us) ||
       (sample.period_us != s_lcdm_last_pwm_period_us) ||
       (pwm_window != s_lcdm_last_servo_pwm_window)))
  {
    (void)snprintf(text, sizeof(text), "%u %u %u",
                   target_pulse_us,
                   sample.raw_pulse_us,
                   pwm_window);
    lcdm_value_cell(0U, text, LCDM_COLOR_BLUE);
    s_lcdm_last_pwm_control_us = target_pulse_us;
    s_lcdm_last_pwm_pulse_us = sample.raw_pulse_us;
    s_lcdm_last_pwm_period_us = sample.period_us;
    s_lcdm_last_servo_pwm_window = pwm_window;
  }

  if (show_target &&
      ((target_mv != s_lcdm_last_servo_target_mv) || (target_adc != s_lcdm_last_servo_target_adc)))
  {
    (void)snprintf(text, sizeof(text), "%u %u", target_adc, target_mv);
    lcdm_value_cell(1U, text, LCDM_COLOR_BLUE);
    s_lcdm_last_servo_target_mv = target_mv;
    s_lcdm_last_servo_target_adc = target_adc;
  }
  else if (!show_target && (s_lcdm_last_servo_target_adc != 0xFFFEU))
  {
    lcdm_value_cell(1U, "--", LCDM_COLOR_RED);
    s_lcdm_last_servo_target_adc = 0xFFFEU;
    s_lcdm_last_servo_target_mv = 0xFFFEU;
  }

  if ((feedback_adc != s_lcdm_last_servo_feedback_adc) ||
      (adc_window != s_lcdm_last_servo_adc_window))
  {
    (void)snprintf(text, sizeof(text), "%u %u %u", raw_adc, feedback_adc, adc_window);
    lcdm_value_cell(2U, text, LCDM_COLOR_BLUE);
    s_lcdm_last_servo_feedback_adc = feedback_adc;
    s_lcdm_last_servo_adc_window = adc_window;
  }

  if (show_target &&
      ((error_adc != s_lcdm_last_servo_error_adc) ||
       (s_lcdm_servo_overshoot_adc != s_lcdm_last_servo_overshoot_adc)))
  {
    (void)snprintf(text, sizeof(text), "%d %u", error_adc, s_lcdm_servo_overshoot_adc);
    lcdm_value_cell(3U, text, (error_adc < 0) ? LCDM_COLOR_RED : LCDM_COLOR_BLUE);
    s_lcdm_last_servo_error_adc = error_adc;
    s_lcdm_last_servo_overshoot_adc = s_lcdm_servo_overshoot_adc;
  }
  else if (!show_target && (s_lcdm_last_servo_error_adc != 32766))
  {
    lcdm_value_cell(3U, "--", LCDM_COLOR_RED);
    s_lcdm_last_servo_error_adc = 32766;
    s_lcdm_last_servo_overshoot_adc = 0xFFFFU;
  }

  if (direction != s_lcdm_last_pwm_direction)
  {
    s_lcdm_last_pwm_direction = direction;
    s_lcdm_last_pwm_duty = 32767;
  }

  if (fresh)
  {
    state_code = s_lcdm_servo_current_state;
    if ((range_state == LCDM_PWM_RANGE_LOW) && range_lock &&
        (state_code != LCDM_SERVO_STATE_DIR_ALARM) &&
        (state_code != LCDM_SERVO_STATE_ADC_LOST))
    {
      state_code = LCDM_SERVO_STATE_LOW_LIMIT;
    }
    else if ((range_state == LCDM_PWM_RANGE_HIGH) && range_lock &&
             (state_code != LCDM_SERVO_STATE_DIR_ALARM) &&
             (state_code != LCDM_SERVO_STATE_ADC_LOST))
    {
      state_code = LCDM_SERVO_STATE_HIGH_LIMIT;
    }
  }
  else
  {
    state_code = has_edge ? LCDM_SERVO_STATE_BAD : LCDM_SERVO_STATE_NO_PWM;
  }

  if ((state_code != s_lcdm_last_pwm_state) ||
      (duty != s_lcdm_last_pwm_duty) ||
      (adc_velocity != s_lcdm_last_servo_velocity) ||
      (event != s_lcdm_last_servo_event))
  {
    const char *state_text;
    switch (state_code)
    {
      case LCDM_SERVO_STATE_HOLD:
        state_text = "H";
        break;
	      case LCDM_SERVO_STATE_DRIVE:
	        state_text = (event == LCDM_SERVO_V3_EVENT_LOCK) ? "S" : ((direction > 0) ? "+" : "-");
	        break;
      case LCDM_SERVO_STATE_BRAKE:
        state_text = "B";
        break;
      case LCDM_SERVO_STATE_BAD:
        state_text = "X";
        break;
      case LCDM_SERVO_STATE_DIR_ALARM:
        state_text = "A";
        break;
      case LCDM_SERVO_STATE_LOW_LIMIT:
        state_text = "LO";
        break;
      case LCDM_SERVO_STATE_HIGH_LIMIT:
        state_text = "HI";
        break;
      case LCDM_SERVO_STATE_RANGE_OFF:
        state_text = "R";
        break;
      case LCDM_SERVO_STATE_ADC_LOST:
        state_text = "AD";
        break;
      default:
        state_text = has_edge ? "ST" : "N";
        break;
    }
    if ((state_code == LCDM_SERVO_STATE_DRIVE) ||
        (state_code == LCDM_SERVO_STATE_LOW_LIMIT) ||
        (state_code == LCDM_SERVO_STATE_HIGH_LIMIT))
    {
      (void)snprintf(text, sizeof(text), "%s%u %d", state_text, abs_duty, adc_velocity);
    }
    else
    {
      (void)snprintf(text, sizeof(text), "%s %d", state_text, adc_velocity);
    }
    lcdm_value_cell(4U, text, (state_code == LCDM_SERVO_STATE_DRIVE) ? LCDM_COLOR_BLUE : LCDM_COLOR_RED);
    s_lcdm_last_pwm_state = state_code;
    s_lcdm_last_pwm_duty = duty;
    s_lcdm_last_servo_velocity = adc_velocity;
    s_lcdm_last_servo_event = event;
  }
}

static bool lcdm_runtime_param_table(LcdmRuntimePage page, LcdmRuntimeParam **params, uint8_t *count)
{
  switch (page)
  {
    case LCDM_PAGE_STEP:
      *params = s_lcdm_step_params;
      *count = (uint8_t)LCDM_ARRAY_COUNT(s_lcdm_step_params);
      return true;

    case LCDM_PAGE_REG:
      *params = s_lcdm_reg_params;
      *count = (uint8_t)LCDM_ARRAY_COUNT(s_lcdm_reg_params);
      return true;

    case LCDM_PAGE_DRIVER:
      *params = s_lcdm_driver_params;
      *count = (uint8_t)LCDM_ARRAY_COUNT(s_lcdm_driver_params);
      return true;

    case LCDM_PAGE_TEMPLATE:
      *params = s_lcdm_template_params;
      *count = (uint8_t)LCDM_ARRAY_COUNT(s_lcdm_template_params);
      return true;

    default:
      return false;
  }
}

static void lcdm_runtime_redraw_param_rows(LcdmRuntimeParam *params,
                                           uint8_t count,
                                           uint8_t previous_selected,
                                           uint8_t selected)
{
  lcdm_page_batch_begin();
  if ((previous_selected != selected) && (previous_selected < count))
  {
    lcdm_param_row(previous_selected, &params[previous_selected], false);
  }
  if (selected < count)
  {
    lcdm_param_row(selected, &params[selected], true);
  }
  lcdm_page_batch_end();
}

static void lcdm_runtime_draw_page(const LcdmRuntimeState *state)
{
  lcdm_page_batch_begin();
  switch (state->page)
  {
    case LCDM_PAGE_MENU:
      lcdm_draw_p01();
      break;

    case LCDM_PAGE_STEP:
      lcdm_draw_param_page("P31",
                           "BRK",
                           s_lcdm_step_params,
                           (uint8_t)LCDM_ARRAY_COUNT(s_lcdm_step_params),
                           state->selected);
      break;

    case LCDM_PAGE_REG:
      lcdm_draw_param_page("P30",
                           "TUN",
                           s_lcdm_reg_params,
                           (uint8_t)LCDM_ARRAY_COUNT(s_lcdm_reg_params),
                           state->selected);
      break;

    case LCDM_PAGE_DRIVER:
      lcdm_draw_param_page("P40",
                           "HBR",
                           s_lcdm_driver_params,
                           (uint8_t)LCDM_ARRAY_COUNT(s_lcdm_driver_params),
                           state->selected);
      break;

    case LCDM_PAGE_TEMPLATE:
      lcdm_draw_param_page("P70",
                           "HLD",
                           s_lcdm_template_params,
                           (uint8_t)LCDM_ARRAY_COUNT(s_lcdm_template_params),
                           state->selected);
      break;

    case LCDM_PAGE_DIAG:
    default:
      lcdm_draw_p50(state->monitor_step);
      break;
  }
  lcdm_page_batch_end();
  pa7_tjc_write_cmd("sendxy=1");
  pb2_tjc_drain_rx();
}

static bool lcdm_runtime_handle_touch(LcdmRuntimeState *state, const LcdmRuntimeTouch *touch)
{
  bool is_press = (touch->event == 1U);
  LcdmRuntimeParam *params = NULL;
  uint8_t count = 0U;
  int8_t row;

  if (!is_press)
  {
    return false;
  }

  state->touch_count++;
  state->last_x = touch->x;
  state->last_y = touch->y;

  if (state->page == LCDM_PAGE_DIAG)
  {
    if (touch->y >= LCDM_FOOTER_TOUCH_Y)
    {
      if (touch->x < LCDM_FOOTER_SEG_W)
      {
        state->page = LCDM_PAGE_MENU;
      }
      else if (touch->x < (LCDM_FOOTER_SEG_W * 2U))
      {
        s_lcdm_pwm_source = (s_lcdm_pwm_source == LCDM_PWM_SOURCE_EXTERNAL) ?
                            LCDM_PWM_SOURCE_INTERNAL :
                            LCDM_PWM_SOURCE_EXTERNAL;
        lcdm_reset_pwm_live_cache();
      }
      else
      {
        state->page = LCDM_PAGE_DRIVER;
      }
      state->selected = 0U;
      return true;
    }
    return false;
  }

  if (state->page == LCDM_PAGE_MENU)
  {
    if (touch->y >= LCDM_FOOTER_TOUCH_Y)
    {
      state->page = LCDM_PAGE_DIAG;
      state->selected = 0U;
      return true;
    }

    row = lcdm_menu_row_from_xy(touch->x, touch->y, LCDM_MENU_VISIBLE_COUNT);
    if (row < 0)
    {
      return false;
    }

    switch ((uint8_t)row)
    {
      case 1U:
        state->page = LCDM_PAGE_STEP;
        break;
      case 3U:
        state->page = LCDM_PAGE_REG;
        break;
      case 4U:
        state->page = LCDM_PAGE_DRIVER;
        break;
      case 5U:
        state->page = LCDM_PAGE_DIAG;
        break;
      case 7U:
        state->page = LCDM_PAGE_TEMPLATE;
        break;
      default:
        return false;
    }
    state->selected = 0U;
    return true;
  }

  if (lcdm_runtime_param_table(state->page, &params, &count))
  {
    uint8_t previous_selected = state->selected;

    if (touch->y >= LCDM_FOOTER_TOUCH_Y)
    {
      if (touch->x < LCDM_FOOTER_SEG_W)
      {
        lcdm_adjust_param(params, count, state->selected, -1);
        lcdm_runtime_redraw_param_rows(params, count, previous_selected, state->selected);
        return false;
      }
      else if (touch->x < (LCDM_FOOTER_SEG_W * 2U))
      {
        state->page = LCDM_PAGE_MENU;
        state->selected = 0U;
        return true;
      }
      else
      {
        lcdm_adjust_param(params, count, state->selected, 1);
        lcdm_runtime_redraw_param_rows(params, count, previous_selected, state->selected);
        return false;
      }
    }

    row = lcdm_row_from_y(touch->y, (count < LCDM_VISIBLE_ROWS) ? count : LCDM_VISIBLE_ROWS);
    if (row >= 0)
    {
      state->selected = (uint8_t)row;
      if (touch->x < LCDM_FOOTER_SEG_W)
      {
        lcdm_adjust_param(params, count, state->selected, -1);
      }
      else if (touch->x > (LCDM_FOOTER_SEG_W * 2U))
      {
        lcdm_adjust_param(params, count, state->selected, 1);
      }
      lcdm_runtime_redraw_param_rows(params, count, previous_selected, state->selected);
      return false;
    }
  }

  return false;
}

static void run_lcdm_runtime_ui_test(void)
{
  uint32_t last_pwm_update_ms = 0U;
  LcdmRuntimeState state = {0};

  Board_Init();
  Servo_Params_Init();
  ADC_Feedback_Init();
#if !SERVO_ENABLE_INTERNAL_STEP_TEST
  PWM_Input_Init();
#endif
  HBridge_Init();
  HBridge_SetFrequency(1000U);
  Board_MotorTimerInit();
#if SERVO_TJC_SOFT_TX
  pa7_tjc_soft_tx_init();
#endif
  s_lcdm_runtime_last_motor_ms = HAL_GetTick();
  s_lcdm_runtime_control_ready = true;
  pa7_tjc_delay_ms_with_led(500U);
  pa7_tjc_force_default_baud();
  pa7_tjc_runtime_init_display();
  state.page = LCDM_PAGE_DIAG;
  state.selected = 0U;
  state.menu_pressed_row = LCDM_MENU_ROW_NONE;
  (void)snprintf(state.status, sizeof(state.status), "%s", lcdm_input_mode_text());
  lcdm_runtime_draw_page(&state);

	  while (1)
	  {
	    LcdmRuntimeTouch touch;
	    if (lcdm_read_touch(&touch, 1U))
	    {
        if (lcdm_runtime_handle_touch(&state, &touch))
        {
          lcdm_runtime_draw_page(&state);
        }
        lcdm_runtime_service_motor_control();
        Board_StatusHeartbeat();
	      continue;
	    }

      uint32_t now_ms = HAL_GetTick();
      lcdm_runtime_service_motor_control();
      if ((state.page == LCDM_PAGE_DIAG) &&
          ((uint32_t)(now_ms - last_pwm_update_ms) >= 150U))
      {
        lcdm_update_pwm_live_values();
        last_pwm_update_ms = now_ms;
      }
      Board_StatusHeartbeat();
  }
}
#endif

#endif

#if SERVO_ENABLE_HBRIDGE_IO_TEST
#ifndef SERVO_IO_TEST_ENABLE_GPIOB
#define SERVO_IO_TEST_ENABLE_GPIOB 0
#endif

#ifndef SERVO_HBRIDGE_IO_TEST_MAX_DUTY
#define SERVO_HBRIDGE_IO_TEST_MAX_DUTY 300U
#endif

#ifndef SERVO_HBRIDGE_IO_TEST_STEP_DUTY
#define SERVO_HBRIDGE_IO_TEST_STEP_DUTY 50U
#endif

#ifndef SERVO_HBRIDGE_IO_TEST_PULSE_MS
#define SERVO_HBRIDGE_IO_TEST_PULSE_MS 180U
#endif

#ifndef SERVO_HBRIDGE_IO_TEST_REST_MS
#define SERVO_HBRIDGE_IO_TEST_REST_MS 900U
#endif

#ifndef SERVO_HBRIDGE_IO_TEST_CYCLE_REST_MS
#define SERVO_HBRIDGE_IO_TEST_CYCLE_REST_MS 2000U
#endif

static void io_test_heartbeat(void)
{
  Board_StatusHeartbeat();
}

static void io_test_wait_ms(uint16_t ms)
{
  uint32_t start = HAL_GetTick();

  while ((uint32_t)(HAL_GetTick() - start) < ms)
  {
    io_test_heartbeat();
  }
}

static void io_test_set_duty(int8_t direction, uint16_t duty)
{
  int16_t signed_duty = 0;

  if (duty > HBRIDGE_DUTY_MAX)
  {
    duty = HBRIDGE_DUTY_MAX;
  }

  if (direction > 0)
  {
    signed_duty = (int16_t)duty;
  }
  else if (direction < 0)
  {
    signed_duty = -(int16_t)duty;
  }

  HBridge_SetSignedDuty(signed_duty);
}

static void io_test_safe_step_pulse(int8_t direction)
{
  uint16_t max_duty = SERVO_HBRIDGE_IO_TEST_MAX_DUTY;
  uint16_t step_duty = SERVO_HBRIDGE_IO_TEST_STEP_DUTY;

  if (max_duty > HBRIDGE_DUTY_MAX)
  {
    max_duty = HBRIDGE_DUTY_MAX;
  }
  if (step_duty == 0U)
  {
    step_duty = 1U;
  }

  for (uint16_t duty = step_duty; duty <= max_duty; duty = (uint16_t)(duty + step_duty))
  {
    io_test_set_duty(direction, duty);
    io_test_wait_ms(SERVO_HBRIDGE_IO_TEST_PULSE_MS);
    HBridge_Coast();
    io_test_wait_ms(SERVO_HBRIDGE_IO_TEST_REST_MS);
  }
}

static void io_test_frequency_cycle(uint16_t frequency_hz)
{
  HBridge_SetFrequency(frequency_hz);
  HBridge_Coast();

  io_test_wait_ms(SERVO_HBRIDGE_IO_TEST_CYCLE_REST_MS);
  io_test_safe_step_pulse(1);
  io_test_wait_ms(SERVO_HBRIDGE_IO_TEST_CYCLE_REST_MS);
  io_test_safe_step_pulse(-1);
  io_test_wait_ms(SERVO_HBRIDGE_IO_TEST_CYCLE_REST_MS);
}

#if SERVO_ENABLE_ADC_LED_TEST
#define ADC_LED_TEST_MIN_RAW      213U
#define ADC_LED_TEST_MAX_RAW      3900U
#define ADC_LED_TEST_CENTER_RAW   ((ADC_LED_TEST_MIN_RAW + ADC_LED_TEST_MAX_RAW) / 2U)
#define ADC_LED_TEST_OFF_BAND     12U

static volatile uint16_t s_adc_led_test_raw;
static volatile int16_t s_adc_led_test_duty;
static volatile uint16_t s_adc_led_test_min_raw = 0xFFFFU;
static volatile uint16_t s_adc_led_test_max_raw;
static volatile uint16_t s_adc_led_test_vdd_mv;
static volatile uint16_t s_adc_led_test_min_mv;
static volatile uint16_t s_adc_led_test_max_mv;

static int16_t io_test_adc_to_signed_duty(uint16_t adc_raw)
{
  uint16_t duty;

  if (adc_raw <= (ADC_LED_TEST_MIN_RAW + ADC_LED_TEST_OFF_BAND))
  {
    return 0;
  }

  if (adc_raw >= ADC_LED_TEST_MAX_RAW)
  {
    return HBRIDGE_DUTY_MAX;
  }

  if (adc_raw < ADC_LED_TEST_CENTER_RAW)
  {
    duty = (uint16_t)((((uint32_t)adc_raw - ADC_LED_TEST_MIN_RAW) * HBRIDGE_DUTY_MAX) /
                      (ADC_LED_TEST_CENTER_RAW - ADC_LED_TEST_MIN_RAW));
    return -(int16_t)duty;
  }

  duty = (uint16_t)((((uint32_t)adc_raw - ADC_LED_TEST_CENTER_RAW) * HBRIDGE_DUTY_MAX) /
                    (ADC_LED_TEST_MAX_RAW - ADC_LED_TEST_CENTER_RAW));
  return (int16_t)duty;
}

static void run_adc_led_test_loop(void)
{
  HBridge_SetFrequency(1000U);

  while (1)
  {
    uint16_t adc_raw = ADC_Feedback_UpdateFiltered();
    int16_t duty = io_test_adc_to_signed_duty(adc_raw);
    uint16_t vdd_mv = ADC_Feedback_GetVddMv();

    s_adc_led_test_raw = adc_raw;
    s_adc_led_test_duty = duty;
    s_adc_led_test_vdd_mv = vdd_mv;
    if (adc_raw < s_adc_led_test_min_raw)
    {
      s_adc_led_test_min_raw = adc_raw;
      s_adc_led_test_min_mv = (uint16_t)(((uint32_t)adc_raw * vdd_mv) / 4095U);
    }
    if (adc_raw > s_adc_led_test_max_raw)
    {
      s_adc_led_test_max_raw = adc_raw;
      s_adc_led_test_max_mv = (uint16_t)(((uint32_t)adc_raw * vdd_mv) / 4095U);
    }
		  HBridge_SetSignedDuty(duty);
    io_test_wait_ms(5U);
  }
}
#endif

static void run_hbridge_io_test(void)
{
  GPIO_InitTypeDef gpio = {0};

  Board_Init();

  __HAL_RCC_GPIOA_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_5;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &gpio);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);

  Servo_Params_Init();
#if SERVO_ENABLE_ADC_LED_TEST
  ADC_Feedback_Init();
#endif
  HBridge_Init();
  HBridge_SetFrequency(1000U);
  Board_MotorTimerInit();

#if SERVO_ENABLE_ADC_LED_TEST
  run_adc_led_test_loop();
#endif

  while (1)
  {
    io_test_frequency_cycle(1000U);
  }
}
#endif

int entry(void)
{
  app_use_flash_vector_table();

#if SERVO_ENABLE_PA7_SQUARE_TEST
  run_pa7_square_test();
#endif

#if SERVO_ENABLE_PA7_TJC_TEST
  run_pa7_tjc_test();
#endif

#if SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
  run_lcdm_runtime_ui_test();
#endif

#if SERVO_ENABLE_HBRIDGE_IO_TEST
  run_hbridge_io_test();
#endif

  /* 主程序入口：先让所有输出进入安全状态，再启动输入/反馈/控制闭环。 */
  Board_Init();
  HBridge_Init();
  Servo_Params_Init();
  ADC_Feedback_Init();
#if !SERVO_ENABLE_INTERNAL_STEP_TEST
  PWM_Input_Init();
#endif
  Servo_Control_Init();
  TJC_LCDM_Init();
  Board_MotorTimerInit();

  uint32_t last_control_ms = HAL_GetTick();

  while (1)
  {
    uint32_t now_ms = HAL_GetTick();
    /* 控制循环固定 1ms 调用一次，避免主循环快慢影响舵机响应。 */
    if ((uint32_t)(now_ms - last_control_ms) >= SERVO_CONTROL_PERIOD_MS)
    {
      last_control_ms = now_ms;
      Servo_Control_Update1ms();
    }

    TJC_LCDM_Process();
    Board_StatusHeartbeat();
  }
}

void APP_ErrorHandler(void)
{
  /* 任何初始化或外设错误都先关闭马达，防止异常状态驱动 H 桥。 */
  HBridge_Coast();
  Board_StatusError();
  while (1)
  {
  }
}
