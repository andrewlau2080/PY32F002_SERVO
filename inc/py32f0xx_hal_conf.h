#ifndef PY32F0XX_HAL_CONF_H
#define PY32F0XX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED

#ifndef SERVO_ENABLE_TJC_LCDM
#define SERVO_ENABLE_TJC_LCDM 0
#endif

#ifndef SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
#define SERVO_ENABLE_LCDM_RUNTIME_UI_TEST 0
#endif

#if SERVO_ENABLE_TJC_LCDM || SERVO_ENABLE_LCDM_RUNTIME_UI_TEST
#define HAL_UART_MODULE_ENABLED
#endif

#if !defined(HSI_VALUE)
#define HSI_VALUE              ((uint32_t)8000000)
#endif
#if !defined(HSE_VALUE)
#define HSE_VALUE              ((uint32_t)24000000)
#endif
#if !defined(HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT    ((uint32_t)200)
#endif
#if !defined(LSI_VALUE)
#define LSI_VALUE              ((uint32_t)32768)
#endif
#if !defined(LSE_VALUE)
#define LSE_VALUE              ((uint32_t)32768)
#endif
#if !defined(LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT    ((uint32_t)5000)
#endif

#define VDD_VALUE              ((uint32_t)3300)
#define PRIORITY_HIGHEST       0
#define PRIORITY_HIGH          1
#define PRIORITY_LOW           2
#define PRIORITY_LOWEST        3
#define TICK_INT_PRIORITY      ((uint32_t)PRIORITY_LOWEST)
#define USE_RTOS               0
#define PREFETCH_ENABLE        0

#ifdef HAL_MODULE_ENABLED
#include "py32f0xx_hal.h"
#endif
#ifdef HAL_RCC_MODULE_ENABLED
#include "py32f0xx_hal_rcc.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
#include "py32f0xx_hal_gpio.h"
#endif
#ifdef HAL_ADC_MODULE_ENABLED
#include "py32f0xx_hal_adc.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
#include "py32f0xx_hal_flash.h"
#endif
#ifdef HAL_TIM_MODULE_ENABLED
#include "py32f0xx_hal_tim.h"
#endif
#ifdef HAL_UART_MODULE_ENABLED
#include "py32f0xx_hal_uart.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
#include "py32f0xx_hal_pwr.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
#include "py32f0xx_hal_cortex.h"
#endif

#define assert_param(expr) ((void)0U)

#ifdef __cplusplus
}
#endif

#endif
