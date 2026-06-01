#include "servo_comm.h"
#include <string.h>
#include "adc_feedback.h"
#include "hbridge.h"
#include "pwm_input.h"
#include "servo_config.h"
#include "servo_control.h"
#include "py32f0xx_hal.h"

#define SERVO_COMM_DEVICE_MAGIC 0x50325356UL
#define SERVO_COMM_FRAME_OVERHEAD 7U

/* 统一构造 ACK/NACK，配置器可根据 status 和 detail 显示具体错误。 */
static uint8_t build_ack(uint8_t command,
                         uint8_t status,
                         uint16_t detail,
                         uint8_t *response,
                         size_t response_capacity,
                         size_t *response_len)
{
  ServoCommAck ack;

  ack.status = status;
  ack.command = command;
  ack.detail = detail;

  if (!Servo_Comm_BuildFrame((status == SERVO_COMM_STATUS_OK) ? SERVO_COMM_CMD_ACK : SERVO_COMM_CMD_NACK,
                             (const uint8_t *)&ack,
                             sizeof(ack),
                             response,
                             response_capacity,
                             response_len))
  {
    return SERVO_COMM_STATUS_BAD_LENGTH;
  }

  return status;
}

uint16_t Servo_Comm_Crc16(const uint8_t *data, size_t len)
{
  /* 通讯帧使用 CRC16/Modbus；参数结构体内部另有 CRC32。 */
  uint16_t crc = 0xFFFFU;

  for (size_t i = 0; i < len; i++)
  {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8U; bit++)
    {
      if ((crc & 1U) != 0U)
      {
        crc = (uint16_t)((crc >> 1U) ^ 0xA001U);
      }
      else
      {
        crc >>= 1U;
      }
    }
  }

  return crc;
}

bool Servo_Comm_BuildFrame(uint8_t command,
                           const uint8_t *payload,
                           uint8_t payload_len,
                           uint8_t *out_frame,
                           size_t out_capacity,
                           size_t *out_len)
{
  /* 帧格式：7E + version + command + len + payload + crc16 + 7E。 */
  if ((payload_len > SERVO_COMM_MAX_PAYLOAD) ||
      (out_capacity < ((size_t)payload_len + SERVO_COMM_FRAME_OVERHEAD)))
  {
    return false;
  }

  out_frame[0] = SERVO_COMM_SOF;
  out_frame[1] = SERVO_COMM_VERSION;
  out_frame[2] = command;
  out_frame[3] = payload_len;
  if ((payload_len > 0U) && (payload != NULL))
  {
    memcpy(&out_frame[4], payload, payload_len);
  }

  uint16_t crc = Servo_Comm_Crc16(&out_frame[1], (size_t)payload_len + 3U);
  out_frame[4U + payload_len] = (uint8_t)(crc & 0xFFU);
  out_frame[5U + payload_len] = (uint8_t)(crc >> 8U);
  out_frame[6U + payload_len] = SERVO_COMM_SOF;
  *out_len = (size_t)payload_len + SERVO_COMM_FRAME_OVERHEAD;

  return true;
}

static bool parse_frame(const uint8_t *frame,
                        size_t frame_len,
                        uint8_t *command,
                        const uint8_t **payload,
                        uint8_t *payload_len)
{
  /* 解析时同时检查帧头/帧尾、版本、长度和 CRC，任一失败都拒绝执行命令。 */
  if ((frame_len < SERVO_COMM_FRAME_OVERHEAD) ||
      (frame[0] != SERVO_COMM_SOF) ||
      (frame[frame_len - 1U] != SERVO_COMM_SOF) ||
      (frame[1] != SERVO_COMM_VERSION))
  {
    return false;
  }

  *command = frame[2];
  *payload_len = frame[3];
  if ((size_t)(*payload_len) + SERVO_COMM_FRAME_OVERHEAD != frame_len)
  {
    return false;
  }

  uint16_t received_crc = (uint16_t)frame[4U + *payload_len] |
                          ((uint16_t)frame[5U + *payload_len] << 8U);
  uint16_t calc_crc = Servo_Comm_Crc16(&frame[1], (size_t)(*payload_len) + 3U);
  if (received_crc != calc_crc)
  {
    return false;
  }

  *payload = &frame[4];
  return true;
}

void Servo_Comm_FillTelemetry(ServoTelemetry *telemetry)
{
  /* 遥测包用于上位机/LCDM 直观看到输入、反馈、输出和参数状态。 */
  ServoStatus status = Servo_Control_GetStatus();
  const ServoParams *params = Servo_Params_Get();
  PWM_InputSample input = PWM_Input_GetSample();
  uint32_t input_age = HAL_GetTick() - input.last_edge_ms;

  telemetry->uptime_ms = HAL_GetTick();
  telemetry->input_us = status.input_us;
  telemetry->input_period_us = input.period_us;
  telemetry->target_adc = status.target_adc;
  telemetry->feedback_adc_raw = ADC_Feedback_GetRaw();
  telemetry->feedback_adc = status.feedback_adc;
  telemetry->error_adc = status.error_adc;
  telemetry->motor_duty = status.motor_duty;
  telemetry->servo_state = (uint8_t)status.state;
  telemetry->hbridge_state = (uint8_t)HBridge_GetState();
  telemetry->input_valid = status.input_valid ? 1U : 0U;
  telemetry->params_valid = Servo_Params_Validate(params) ? 1U : 0U;
  telemetry->params_crc32 = params->crc32;
  telemetry->adc_min_count = params->adc_min_count;
  telemetry->adc_center_count = params->adc_center_count;
  telemetry->adc_max_count = params->adc_max_count;
  telemetry->input_age_ms = (input_age > 65535U) ? 65535U : (uint16_t)input_age;
  telemetry->reserved = 0U;
}

uint8_t Servo_Comm_HandleFrame(const uint8_t *frame,
                               size_t frame_len,
                               uint8_t *response,
                               size_t response_capacity,
                               size_t *response_len)
{
  uint8_t command;
  const uint8_t *payload;
  uint8_t payload_len;

  *response_len = 0U;

  if (!parse_frame(frame, frame_len, &command, &payload, &payload_len))
  {
    return build_ack(0U,
                     SERVO_COMM_STATUS_BAD_FRAME,
                     0U,
                     response,
                     response_capacity,
                     response_len);
  }

  switch (command)
  {
    case SERVO_COMM_CMD_HELLO:
    {
      /* HELLO 是进入配置后的第一步，用于确认协议版本和结构体大小。 */
      ServoCommHello hello;
      if (payload_len != 0U)
      {
        return build_ack(command, SERVO_COMM_STATUS_BAD_LENGTH, payload_len, response, response_capacity, response_len);
      }

      hello.device_magic = SERVO_COMM_DEVICE_MAGIC;
      hello.protocol_version = SERVO_COMM_VERSION;
      hello.fixed_program_version = SERVO_FIXED_PROGRAM_VERSION;
      hello.params_size = sizeof(ServoParams);
      hello.telemetry_size = sizeof(ServoTelemetry);
      hello.capabilities = SERVO_COMM_CAP_READ_PARAMS |
                           SERVO_COMM_CAP_WRITE_PARAMS |
                           SERVO_COMM_CAP_READ_TELEMETRY |
                           SERVO_COMM_CAP_SHARED_PWM_PIN;

      if (!Servo_Comm_BuildFrame(command, (const uint8_t *)&hello, sizeof(hello), response, response_capacity, response_len))
      {
        return SERVO_COMM_STATUS_BAD_LENGTH;
      }
      return SERVO_COMM_STATUS_OK;
    }

    case SERVO_COMM_CMD_READ_PARAMS:
    {
      /* 读出当前运行参数，配置器写入后也必须读回比对。 */
      const ServoParams *params = Servo_Params_Get();
      if (payload_len != 0U)
      {
        return build_ack(command, SERVO_COMM_STATUS_BAD_LENGTH, payload_len, response, response_capacity, response_len);
      }
      if (!Servo_Comm_BuildFrame(command, (const uint8_t *)params, sizeof(*params), response, response_capacity, response_len))
      {
        return SERVO_COMM_STATUS_BAD_LENGTH;
      }
      return SERVO_COMM_STATUS_OK;
    }

    case SERVO_COMM_CMD_WRITE_PARAMS:
    {
      /* 写入必须是一整包 ServoParams；不允许只改局部字段，避免半更新。 */
      ServoParams params;
      if (payload_len != sizeof(params))
      {
        return build_ack(command, SERVO_COMM_STATUS_BAD_LENGTH, payload_len, response, response_capacity, response_len);
      }

      memcpy(&params, payload, sizeof(params));
      if (!Servo_Params_Validate(&params))
      {
        return build_ack(command, SERVO_COMM_STATUS_BAD_PARAMS, 0U, response, response_capacity, response_len);
      }

      if (!Servo_Params_Save(&params))
      {
        return build_ack(command, SERVO_COMM_STATUS_FLASH_ERROR, 0U, response, response_capacity, response_len);
      }

      return build_ack(command, SERVO_COMM_STATUS_OK, 0U, response, response_capacity, response_len);
    }

    case SERVO_COMM_CMD_READ_TELEMETRY:
    {
      /* 调试时周期读取遥测，可重现舵机内部控制状态。 */
      ServoTelemetry telemetry;
      if (payload_len != 0U)
      {
        return build_ack(command, SERVO_COMM_STATUS_BAD_LENGTH, payload_len, response, response_capacity, response_len);
      }
      Servo_Comm_FillTelemetry(&telemetry);
      if (!Servo_Comm_BuildFrame(command, (const uint8_t *)&telemetry, sizeof(telemetry), response, response_capacity, response_len))
      {
        return SERVO_COMM_STATUS_BAD_LENGTH;
      }
      return SERVO_COMM_STATUS_OK;
    }

    case SERVO_COMM_CMD_EXIT_CONFIG:
      return build_ack(command, SERVO_COMM_STATUS_OK, 0U, response, response_capacity, response_len);

    default:
      return build_ack(command, SERVO_COMM_STATUS_BAD_COMMAND, command, response, response_capacity, response_len);
  }
}
