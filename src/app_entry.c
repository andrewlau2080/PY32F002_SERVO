#include "main.h"
#include "adc_feedback.h"
#include "board.h"
#include "hbridge.h"
#include "pwm_input.h"
#include "servo_config.h"
#include "servo_control.h"
#include "servo_params.h"

int entry(void)
{
  /* 主程序入口：先让所有输出进入安全状态，再启动输入/反馈/控制闭环。 */
  Board_Init();
  HBridge_Init();
  Servo_Params_Init();
  ADC_Feedback_Init();
  PWM_Input_Init();
  Servo_Control_Init();
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
  }
}

void APP_ErrorHandler(void)
{
  /* 任何初始化或外设错误都先关闭马达，防止异常状态驱动 H 桥。 */
  HBridge_Coast();
  while (1)
  {
  }
}
