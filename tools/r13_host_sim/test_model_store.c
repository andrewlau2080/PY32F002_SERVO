#include <stdio.h>
#include <string.h>

#include "servo_model_store.h"
#include "py32f0xx_hal.h"

uint8_t g_fake_flash[FLASH_SIZE];
bool g_fake_flash_fail_erase;
bool g_fake_flash_fail_program;
uint32_t g_fake_primask;

static unsigned failures;
static unsigned irq_disable_count;
static unsigned irq_enable_count;

static void check(const char *name, bool pass)
{
  printf("%s: %s\n", pass ? "PASS" : "FAIL", name);
  if (!pass) failures++;
}

HAL_StatusTypeDef HAL_FLASH_Unlock(void) { return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Lock(void) { return HAL_OK; }

HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *erase, uint32_t *page_error)
{
  if (g_fake_flash_fail_erase)
  {
    if (page_error != NULL) *page_error = 0U;
    return HAL_ERROR;
  }
  memset(&g_fake_flash[erase->PageAddress - FLASH_BASE], 0xFF,
         FLASH_PAGE_SIZE * erase->NbPages);
  return HAL_OK;
}

HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type, uint32_t address, uint32_t *data)
{
  if ((type != FLASH_TYPEPROGRAM_PAGE) || g_fake_flash_fail_program) return HAL_ERROR;
  memcpy(&g_fake_flash[address - FLASH_BASE], data, FLASH_PAGE_SIZE);
  return HAL_OK;
}

void fake_hal_disable_irq(void) { irq_disable_count++; g_fake_primask = 1U; }
void fake_hal_enable_irq(void) { irq_enable_count++; g_fake_primask = 0U; }
const void *Fake_ModelFlashPtr(uint32_t address)
{
  return &g_fake_flash[address - FLASH_BASE];
}

static ServoLearnedModel make_model(uint16_t model_id, uint16_t confidence)
{
  ServoLearnedModel model;
  memset(&model, 0, sizeof(model));
  model.model_id = model_id;
  model.profile_id = 3U;
  model.confidence_permille = confidence;
  model.start_duty_forward = 300U;
  model.start_duty_reverse = 320U;
  model.brake_accel_q8_forward = 180U;
  model.brake_accel_q8_reverse = 160U;
  model.calibration_fingerprint = 0xA1234567UL;
  model.reference_motor_supply_mv = 5000U;
  model.motion_sample_count = 32U;
  model.brake_sample_count = 8U;
  model.successful_move_count = 4U;
  return model;
}

int main(void)
{
  memset(g_fake_flash, 0xFF, sizeof(g_fake_flash));
  Servo_ModelStore_Init();
  check("erased flash starts invalid", !Servo_ModelStore_IsValid());

  ServoLearnedModel first = make_model(11U, 700U);
  check("unqualified save rejected", !Servo_ModelStore_Save(&first, false));
  check("rejected save leaves flash erased", g_fake_flash[FLASH_SIZE - 3U * FLASH_PAGE_SIZE] == 0xFFU);
  ServoLearnedModel invalid = first;
  invalid.model_id = 0U;
  check("zero model identity rejected", !Servo_ModelStore_Save(&invalid, true));
  invalid = first;
  invalid.calibration_fingerprint = 0U;
  check("zero calibration fingerprint rejected", !Servo_ModelStore_Save(&invalid, true));
  invalid = first;
  invalid.reference_motor_supply_mv = 0U;
  check("unknown motor supply rejected", !Servo_ModelStore_Save(&invalid, true));
  invalid = first;
  invalid.confidence_permille = 0U;
  check("zero confidence rejected", !Servo_ModelStore_Save(&invalid, true));
  invalid = first;
  invalid.motion_sample_count = 0U;
  invalid.brake_sample_count = 0U;
  check("model with no learning samples rejected", !Servo_ModelStore_Save(&invalid, true));
  invalid = first;
  invalid.successful_move_count = 0U;
  check("model with no successful movement rejected", !Servo_ModelStore_Save(&invalid, true));
  check("invalid models leave flash erased", g_fake_flash[FLASH_SIZE - 3U * FLASH_PAGE_SIZE] == 0xFFU);

  check("first qualified slot saves", Servo_ModelStore_Save(&first, true));
  check("first slot valid generation one", Servo_ModelStore_IsValid() &&
        Servo_ModelStore_Get()->generation == 1U && Servo_ModelStore_Get()->model_id == 11U);
  ServoLearnedModel first_saved = *Servo_ModelStore_Get();

  ServoLearnedModel second = make_model(11U, 800U);
  check("second slot saves", Servo_ModelStore_Save(&second, true));
  check("second generation selected", Servo_ModelStore_Get()->generation == 2U &&
        Servo_ModelStore_Get()->model_id == 11U);
  Servo_ModelStore_InitFor(11U, 3U, 0xA1234567UL, 5000U, 100U);
  check("matching model/profile/calibration/supply loads", Servo_ModelStore_IsValid() &&
        Servo_ModelStore_Get()->model_id == 11U);
  Servo_ModelStore_InitFor(21U, 3U, 0xA1234567UL, 5000U, 100U);
  check("different model is rejected", !Servo_ModelStore_IsValid());
  Servo_ModelStore_InitFor(11U, 4U, 0xA1234567UL, 5000U, 100U);
  check("different profile is rejected", !Servo_ModelStore_IsValid());
  Servo_ModelStore_InitFor(11U, 3U, 0xA1234566UL, 5000U, 100U);
  check("different calibration is rejected", !Servo_ModelStore_IsValid());
  Servo_ModelStore_InitFor(11U, 3U, 0xA1234567UL, 5400U, 100U);
  check("different supply is rejected", !Servo_ModelStore_IsValid());
  Servo_ModelStore_InitFor(11U, 3U, 0xA1234567UL, 0U, 100U);
  check("unknown supply fails closed", !Servo_ModelStore_IsValid());
  Servo_ModelStore_Init();
  check("context-free initializer fails closed", !Servo_ModelStore_IsValid());

  Servo_ModelStore_InitFor(11U, 3U, 0xA1234567UL, 5000U, 100U);
  ((volatile ServoLearnedModel *)Servo_ModelStore_Get())->crc32 ^= 1U;
  Servo_ModelStore_InitFor(11U, 3U, 0xA1234567UL, 5000U, 100U);
  check("corrupt newest CRC falls back to old slot", Servo_ModelStore_IsValid() &&
        Servo_ModelStore_Get()->generation == first_saved.generation &&
        Servo_ModelStore_Get()->model_id == first_saved.model_id);

  ServoLearnedModel third = make_model(33U, 900U);
  check("new save replaces corrupt inactive slot", Servo_ModelStore_Save(&third, true));
  ServoLearnedModel active_before_failure = *Servo_ModelStore_Get();

  ServoLearnedModel fourth = make_model(44U, 950U);
  g_fake_flash_fail_program = true;
  check("program failure reported", !Servo_ModelStore_Save(&fourth, true));
  g_fake_flash_fail_program = false;
  check("program failure preserves active record", Servo_ModelStore_Get()->generation == active_before_failure.generation &&
        Servo_ModelStore_Get()->model_id == active_before_failure.model_id);
  Servo_ModelStore_InitFor(33U, 3U, 0xA1234567UL, 5000U, 100U);
  check("restart recovers previous valid slot after program failure", Servo_ModelStore_IsValid() &&
        Servo_ModelStore_Get()->model_id == active_before_failure.model_id);

  g_fake_flash_fail_erase = true;
  check("erase failure reported", !Servo_ModelStore_Save(&fourth, true));
  g_fake_flash_fail_erase = false;
  check("erase failure leaves active record selected", Servo_ModelStore_Get()->model_id == active_before_failure.model_id);
  unsigned disable_before = irq_disable_count;
  unsigned enable_before = irq_enable_count;
  check("save succeeds with interrupts initially enabled", Servo_ModelStore_Save(&fourth, true));
  check("enabled interrupt state is restored", g_fake_primask == 0U &&
        irq_disable_count == disable_before + 1U && irq_enable_count == enable_before + 1U);

  g_fake_primask = 1U;
  disable_before = irq_disable_count;
  enable_before = irq_enable_count;
  check("save succeeds when caller interrupts are already disabled", Servo_ModelStore_Save(&third, true));
  check("pre-disabled interrupt state remains disabled", g_fake_primask == 1U &&
        irq_disable_count == disable_before + 1U && irq_enable_count == enable_before);
  g_fake_primask = 0U;
  check("test harness clean", failures == 0U);
  return failures ? 1 : 0;
}
