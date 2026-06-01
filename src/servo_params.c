#include "servo_params.h"
#include "servo_config.h"
#include "py32f0xx_hal.h"

#define SERVO_PARAMS_MAGIC       0x53525650UL
#define SERVO_PARAMS_VERSION     1U
#define SERVO_PARAMS_FLASH_ADDR  (FLASH_BASE + FLASH_SIZE - FLASH_PAGE_SIZE)

static ServoParams s_params;

/* 参数保存使用 CRC32；计算时把 crc32 字段本身当 0 处理。 */
static uint32_t crc32_update(uint32_t crc, uint8_t data)
{
  crc ^= data;
  for (uint32_t i = 0; i < 8U; i++)
  {
    if ((crc & 1U) != 0U)
    {
      crc = (crc >> 1U) ^ 0xEDB88320UL;
    }
    else
    {
      crc >>= 1U;
    }
  }
  return crc;
}

uint32_t Servo_Params_Crc32(const ServoParams *params)
{
  const uint8_t *bytes = (const uint8_t *)params;
  uint32_t crc = 0xFFFFFFFFUL;
  uint32_t crc_offset = (uint32_t)((const uint8_t *)&params->crc32 - bytes);

  for (uint32_t i = 0; i < sizeof(*params); i++)
  {
    if ((i >= crc_offset) && (i < (crc_offset + sizeof(params->crc32))))
    {
      crc = crc32_update(crc, 0U);
    }
    else
    {
      crc = crc32_update(crc, bytes[i]);
    }
  }

  return ~crc;
}

void Servo_Params_GetDefaults(ServoParams *params)
{
  /* 出厂默认参数：没有合法 Flash 参数时使用，也作为上位机模板。 */
  params->magic = SERVO_PARAMS_MAGIC;
  params->version = SERVO_PARAMS_VERSION;
  params->size = sizeof(*params);
  params->pulse_lower_us = 900U;
  params->pulse_center_us = 1500U;
  params->pulse_high_us = 2100U;
  params->adc_min_count = SERVO_ADC_MIN_COUNT;
  params->adc_center_count = SERVO_ADC_CENTER_COUNT;
  params->adc_max_count = SERVO_ADC_MAX_COUNT;
  params->neutral_offset_count = 0;
  params->left_range_count = 0;
  params->right_range_count = 0;
  params->deadband_us = 2U;
  params->deadband_count_min = SERVO_ADC_DEADBAND_COUNT;
  params->stretcher_q8 = 384U;
  params->max_duty = SERVO_CONTROL_MAX_DUTY;
  params->boost_duty = SERVO_CONTROL_MIN_DUTY;
  params->brake_band_count = SERVO_CONTROL_BRAKE_BAND_COUNT;
  params->drive_frequency_hz = 1000U;
  params->forward_speed_q8 = 256U;
  params->reverse_speed_q8 = 256U;
  params->soft_start_step_count = 8U;
  params->stall_duty_threshold = SERVO_STALL_DUTY_THRESHOLD;
  params->stall_min_move_count = SERVO_STALL_MIN_MOVE_COUNT;
  params->stall_time_ms = SERVO_STALL_TIME_MS;
  params->stall_recovery_ms = SERVO_STALL_RECOVERY_MS;
  params->input_timeout_ms = SERVO_INPUT_TIMEOUT_MS;
  params->flags = SERVO_PARAM_FLAG_OL_PROTECT |
                  SERVO_PARAM_FLAG_SOFT_START |
                  SERVO_PARAM_FLAG_LOSE_PPM_LOCK;
  params->crc32 = 0U;
  params->crc32 = Servo_Params_Crc32(params);
}

bool Servo_Params_Validate(const ServoParams *params)
{
  /* 先检查身份、版本、长度和 CRC，再检查各参数是否在安全范围内。 */
  if ((params->magic != SERVO_PARAMS_MAGIC) ||
      (params->version != SERVO_PARAMS_VERSION) ||
      (params->size != sizeof(*params)) ||
      (params->crc32 != Servo_Params_Crc32(params)))
  {
    return false;
  }

  if ((params->pulse_lower_us < 500U) ||
      (params->pulse_high_us > 2500U) ||
      (params->pulse_lower_us >= params->pulse_center_us) ||
      (params->pulse_center_us >= params->pulse_high_us))
  {
    return false;
  }

  if ((params->adc_min_count >= params->adc_center_count) ||
      (params->adc_center_count >= params->adc_max_count) ||
      (params->adc_max_count > 4095U))
  {
    return false;
  }

  if ((params->max_duty == 0U) ||
      (params->max_duty > HBRIDGE_DUTY_MAX) ||
      (params->boost_duty > params->max_duty) ||
      (params->drive_frequency_hz < 100U) ||
      (params->drive_frequency_hz > 20000U))
  {
    return false;
  }

  return true;
}

void Servo_Params_Init(void)
{
  const ServoParams *flash_params = (const ServoParams *)SERVO_PARAMS_FLASH_ADDR;

  /* 上电优先使用最后一页 Flash 参数；无效则回退到默认值。 */
  if (Servo_Params_Validate(flash_params))
  {
    s_params = *flash_params;
  }
  else
  {
    Servo_Params_GetDefaults(&s_params);
  }
}

const ServoParams *Servo_Params_Get(void)
{
  return &s_params;
}

bool Servo_Params_Save(const ServoParams *params)
{
  ServoParams checked = *params;
  uint32_t page_buffer[FLASH_PAGE_SIZE / sizeof(uint32_t)];
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t page_error = 0U;

  checked.magic = SERVO_PARAMS_MAGIC;
  checked.version = SERVO_PARAMS_VERSION;
  checked.size = sizeof(checked);
  checked.crc32 = 0U;
  checked.crc32 = Servo_Params_Crc32(&checked);

  if (!Servo_Params_Validate(&checked))
  {
    return false;
  }

  for (uint32_t i = 0; i < (FLASH_PAGE_SIZE / sizeof(uint32_t)); i++)
  {
    page_buffer[i] = 0xFFFFFFFFUL;
  }

  const uint32_t *src = (const uint32_t *)&checked;
  for (uint32_t i = 0; i < ((sizeof(checked) + 3U) / sizeof(uint32_t)); i++)
  {
    page_buffer[i] = src[i];
  }

  erase.TypeErase = FLASH_TYPEERASE_PAGEERASE;
  erase.PageAddress = SERVO_PARAMS_FLASH_ADDR;
  erase.NbPages = 1U;

  /* 写 Flash 时关闭中断，避免擦写期间控制中断继续访问异常时序。 */
  __disable_irq();
  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    __enable_irq();
    return false;
  }
  if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK)
  {
    (void)HAL_FLASH_Lock();
    __enable_irq();
    return false;
  }
  if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_PAGE, SERVO_PARAMS_FLASH_ADDR, page_buffer) != HAL_OK)
  {
    (void)HAL_FLASH_Lock();
    __enable_irq();
    return false;
  }
  (void)HAL_FLASH_Lock();
  __enable_irq();

  s_params = checked;
  return true;
}

bool Servo_Params_FlagEnabled(uint32_t flag)
{
  return ((s_params.flags & flag) != 0U);
}
