#include "main.h"
#include "tjc_lcdm.h"

#if SERVO_ENABLE_TJC_LCDM || SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
#include "py32f0xx_hal_uart.h"

#ifndef SERVO_TJC_UART_ALT_PINS
#define SERVO_TJC_UART_ALT_PINS 0
#endif

#ifndef SERVO_TJC_SOFT_TX
#define SERVO_TJC_SOFT_TX 0
#endif
#endif

void HAL_MspInit(void)
{
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1)
  {
    __HAL_RCC_ADC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
  }
}

#if SERVO_ENABLE_TJC_LCDM || SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
  GPIO_InitTypeDef gpio = {0};

  if (huart->Instance == USART1)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
#if SERVO_TJC_UART_ALT_PINS || SERVO_TJC_SOFT_TX
    __HAL_RCC_GPIOB_CLK_ENABLE();
#endif
    __HAL_RCC_USART1_CLK_ENABLE();

#if SERVO_TJC_SOFT_TX
    /* PA7 is driven by the software-TX path. Keep hardware USART off PA3 so ADC feedback is not stolen. */
    gpio.Pin = GPIO_PIN_2;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF0_USART1;
    HAL_GPIO_Init(GPIOB, &gpio);
#elif SERVO_TJC_UART_ALT_PINS
    gpio.Pin = GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF8_USART1;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_2;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF0_USART1;
    HAL_GPIO_Init(GPIOB, &gpio);
#else
    gpio.Pin = GPIO_PIN_2;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF1_USART1;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_3;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF1_USART1;
    HAL_GPIO_Init(GPIOA, &gpio);
#endif

#if SERVO_ENABLE_TJC_LCDM
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
#endif
  }
}
#endif
