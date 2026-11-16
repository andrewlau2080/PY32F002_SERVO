#ifndef PY32F0XX_HAL_H
#define PY32F0XX_HAL_H

#include <stdbool.h>
#include <stdint.h>

#define FLASH_SIZE 20480U
#define FLASH_PAGE_SIZE 128U
#define FLASH_TYPEERASE_PAGEERASE 1U
#define FLASH_TYPEPROGRAM_PAGE 2U
#define FLASH_BASE 0x08000000UL

extern uint8_t g_fake_flash[FLASH_SIZE];
extern bool g_fake_flash_fail_erase;
extern bool g_fake_flash_fail_program;
extern uint32_t g_fake_primask;
const void *Fake_ModelFlashPtr(uint32_t address);

typedef struct
{
  uint32_t TypeErase;
  uint32_t PageAddress;
  uint32_t NbPages;
} FLASH_EraseInitTypeDef;

typedef enum { HAL_OK = 0, HAL_ERROR = 1 } HAL_StatusTypeDef;

HAL_StatusTypeDef HAL_FLASH_Unlock(void);
HAL_StatusTypeDef HAL_FLASH_Lock(void);
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *erase, uint32_t *page_error);
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type, uint32_t address, uint32_t *data);
void fake_hal_disable_irq(void);
void fake_hal_enable_irq(void);
#define __get_PRIMASK() g_fake_primask

#define __disable_irq() fake_hal_disable_irq()
#define __enable_irq() fake_hal_enable_irq()

#endif
