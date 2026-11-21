#include "servo_model_store.h"
#include "py32f0xx_hal.h"

#include <string.h>

#define SERVO_MODEL_MAGIC       0x4D4F4453UL
#define SERVO_MODEL_VERSION     1U
#define SERVO_MODEL_SLOT_A_ADDR (FLASH_BASE + FLASH_SIZE - (3U * FLASH_PAGE_SIZE))
#define SERVO_MODEL_SLOT_B_ADDR (FLASH_BASE + FLASH_SIZE - (2U * FLASH_PAGE_SIZE))
#ifndef SERVO_MODEL_FLASH_PTR
#define SERVO_MODEL_FLASH_PTR(address) ((const ServoLearnedModel *)(uintptr_t)(address))
#endif

_Static_assert((FLASH_PAGE_SIZE == 128U), "Learned-model layout expects 128-byte pages");
_Static_assert((sizeof(ServoLearnedModel) <= FLASH_PAGE_SIZE), "Learned model must fit one flash page");

static const ServoLearnedModel s_empty_model;
static const ServoLearnedModel *s_model_ptr = &s_empty_model;
static int8_t s_active_slot = -1;
static bool s_model_valid;

static void restore_irq_state(uint32_t previous_primask)
{
  if ((previous_primask & 1U) == 0U)
  {
    __enable_irq();
  }
}

static uint32_t crc32_update(uint32_t crc, uint8_t byte)
{
  crc ^= byte;
  for (uint32_t bit = 0U; bit < 8U; bit++)
  {
    crc = ((crc & 1U) != 0U) ? ((crc >> 1U) ^ 0xEDB88320UL) : (crc >> 1U);
  }
  return crc;
}

static uint32_t model_crc32(const ServoLearnedModel *model)
{
  const uint8_t *bytes = (const uint8_t *)model;
  const uint32_t crc_offset = (uint32_t)((const uint8_t *)&model->crc32 - bytes);
  uint32_t crc = 0xFFFFFFFFUL;

  for (uint32_t i = 0U; i < sizeof(*model); i++)
  {
    crc = crc32_update(crc, ((i >= crc_offset) &&
                             (i < (crc_offset + sizeof(model->crc32)))) ? 0U : bytes[i]);
  }
  return ~crc;
}

bool Servo_ModelStore_Validate(const ServoLearnedModel *model)
{
  if ((model == NULL) ||
      (model->magic != SERVO_MODEL_MAGIC) ||
      (model->version != SERVO_MODEL_VERSION) ||
      (model->size != sizeof(*model)) ||
      (model->confidence_permille > SERVO_MODEL_CONFIDENCE_MAX) ||
      (model->start_duty_forward > 1000U) ||
      (model->start_duty_reverse > 1000U) ||
      (model->crc32 != model_crc32(model)))
  {
    return false;
  }
  return true;
}

static bool generation_is_newer(uint32_t a, uint32_t b)
{
  return ((int32_t)(a - b) > 0);
}

static bool model_matches_context(const ServoLearnedModel *model,
                                  uint16_t expected_model_id,
                                  uint16_t expected_profile_id,
                                  uint32_t expected_calibration_fingerprint,
                                  uint16_t expected_motor_supply_mv,
                                  uint16_t supply_tolerance_mv)
{
  uint16_t supply_delta;
  if ((model == NULL) ||
      (expected_model_id == 0U) ||
      (expected_profile_id == 0U) ||
      (expected_calibration_fingerprint == 0U) ||
      (expected_motor_supply_mv == 0U) ||
      (model->model_id != expected_model_id) ||
      (model->profile_id != expected_profile_id) ||
      (model->calibration_fingerprint != expected_calibration_fingerprint) ||
      (model->reference_motor_supply_mv == 0U))
  {
    return false;
  }
  supply_delta = (model->reference_motor_supply_mv > expected_motor_supply_mv) ?
      (uint16_t)(model->reference_motor_supply_mv - expected_motor_supply_mv) :
      (uint16_t)(expected_motor_supply_mv - model->reference_motor_supply_mv);
  return supply_delta <= supply_tolerance_mv;
}

void Servo_ModelStore_Init(void)
{
  /* Legacy context-free initialization must never enable an unverified model. */
  Servo_ModelStore_InitFor(0U, 0U, 0U, 0U, 0U);
}

void Servo_ModelStore_InitFor(uint16_t expected_model_id,
                              uint16_t expected_profile_id,
                              uint32_t expected_calibration_fingerprint,
                              uint16_t expected_motor_supply_mv,
                              uint16_t supply_tolerance_mv)
{
  const ServoLearnedModel *slot_a = SERVO_MODEL_FLASH_PTR(SERVO_MODEL_SLOT_A_ADDR);
  const ServoLearnedModel *slot_b = SERVO_MODEL_FLASH_PTR(SERVO_MODEL_SLOT_B_ADDR);
  bool valid_a = Servo_ModelStore_Validate(slot_a) &&
      model_matches_context(slot_a, expected_model_id, expected_profile_id,
                            expected_calibration_fingerprint, expected_motor_supply_mv,
                            supply_tolerance_mv);
  bool valid_b = Servo_ModelStore_Validate(slot_b) &&
      model_matches_context(slot_b, expected_model_id, expected_profile_id,
                            expected_calibration_fingerprint, expected_motor_supply_mv,
                            supply_tolerance_mv);

  s_active_slot = -1;
  s_model_valid = false;
  s_model_ptr = &s_empty_model;
  if (valid_a && valid_b)
  {
    if (generation_is_newer(slot_b->generation, slot_a->generation))
    {
      s_model_ptr = slot_b;
      s_active_slot = 1;
    }
    else
    {
      s_model_ptr = slot_a;
      s_active_slot = 0;
    }
    s_model_valid = true;
  }
  else if (valid_a)
  {
    s_model_ptr = slot_a;
    s_active_slot = 0;
    s_model_valid = true;
  }
  else if (valid_b)
  {
    s_model_ptr = slot_b;
    s_active_slot = 1;
    s_model_valid = true;
  }
}

const ServoLearnedModel *Servo_ModelStore_Get(void)
{
  return s_model_ptr;
}

bool Servo_ModelStore_IsValid(void)
{
  return s_model_valid;
}

bool Servo_ModelStore_Save(const ServoLearnedModel *model,
                           bool qualified_and_stationary)
{
  if ((!qualified_and_stationary) || (model == NULL) ||
      (model->model_id == 0U) ||
      (model->profile_id == 0U) ||
      (model->calibration_fingerprint == 0U) ||
      (model->reference_motor_supply_mv == 0U) ||
      (model->confidence_permille == 0U) ||
      ((model->motion_sample_count == 0U) && (model->brake_sample_count == 0U)) ||
      (model->successful_move_count == 0U))
  {
    return false;
  }

  ServoLearnedModel checked = *model;
  uint32_t page_buffer[FLASH_PAGE_SIZE / sizeof(uint32_t)];
  uint32_t target_addr;
  int8_t target_slot;
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t page_error = 0U;
  uint32_t previous_primask;

  checked.magic = SERVO_MODEL_MAGIC;
  checked.version = SERVO_MODEL_VERSION;
  checked.size = sizeof(checked);
  checked.generation = s_model_valid ? (s_model_ptr->generation + 1U) : 1U;
  checked.crc32 = 0U;
  checked.crc32 = model_crc32(&checked);
  if (!Servo_ModelStore_Validate(&checked))
  {
    return false;
  }

  target_slot = (s_active_slot == 0) ? 1 : 0;
  target_addr = (target_slot == 0) ? SERVO_MODEL_SLOT_A_ADDR : SERVO_MODEL_SLOT_B_ADDR;
  for (uint32_t i = 0U; i < (FLASH_PAGE_SIZE / sizeof(uint32_t)); i++)
  {
    page_buffer[i] = 0xFFFFFFFFUL;
  }
  memcpy(page_buffer, &checked, sizeof(checked));

  erase.TypeErase = FLASH_TYPEERASE_PAGEERASE;
  erase.PageAddress = target_addr;
  erase.NbPages = 1U;

  /* Save only from a confirmed stationary state; keep the previous slot intact until verified. */
  previous_primask = __get_PRIMASK();
  __disable_irq();
  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    restore_irq_state(previous_primask);
    return false;
  }
  if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK)
  {
    (void)HAL_FLASH_Lock();
    restore_irq_state(previous_primask);
    return false;
  }
  if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_PAGE, target_addr, page_buffer) != HAL_OK)
  {
    (void)HAL_FLASH_Lock();
    restore_irq_state(previous_primask);
    return false;
  }
  (void)HAL_FLASH_Lock();
  restore_irq_state(previous_primask);

  if (!Servo_ModelStore_Validate(SERVO_MODEL_FLASH_PTR(target_addr)))
  {
    return false;
  }

  s_model_ptr = SERVO_MODEL_FLASH_PTR(target_addr);
  s_active_slot = target_slot;
  s_model_valid = true;
  return true;
}
