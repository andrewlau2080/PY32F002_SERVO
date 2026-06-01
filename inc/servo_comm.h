#ifndef SERVO_COMM_H
#define SERVO_COMM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "servo_params.h"

#define SERVO_COMM_SOF                 0x7EU
#define SERVO_COMM_VERSION             1U
#define SERVO_COMM_MAX_PAYLOAD         120U
#define SERVO_COMM_MAX_FRAME           128U

#define SERVO_COMM_CMD_HELLO           0x01U
#define SERVO_COMM_CMD_READ_PARAMS     0x02U
#define SERVO_COMM_CMD_WRITE_PARAMS    0x03U
#define SERVO_COMM_CMD_READ_TELEMETRY  0x04U
#define SERVO_COMM_CMD_EXIT_CONFIG     0x05U
#define SERVO_COMM_CMD_ACK             0x80U
#define SERVO_COMM_CMD_NACK            0x81U

#define SERVO_COMM_STATUS_OK           0x00U
#define SERVO_COMM_STATUS_BAD_FRAME    0x01U
#define SERVO_COMM_STATUS_BAD_CRC      0x02U
#define SERVO_COMM_STATUS_BAD_COMMAND  0x03U
#define SERVO_COMM_STATUS_BAD_LENGTH   0x04U
#define SERVO_COMM_STATUS_BAD_PARAMS   0x05U
#define SERVO_COMM_STATUS_FLASH_ERROR  0x06U

#define SERVO_COMM_CAP_READ_PARAMS     (1UL << 0)
#define SERVO_COMM_CAP_WRITE_PARAMS    (1UL << 1)
#define SERVO_COMM_CAP_READ_TELEMETRY  (1UL << 2)
#define SERVO_COMM_CAP_SHARED_PWM_PIN  (1UL << 3)

typedef struct
{
  uint32_t device_magic;
  uint16_t protocol_version;
  uint16_t fixed_program_version;
  uint16_t params_size;
  uint16_t telemetry_size;
  uint32_t capabilities;
} ServoCommHello;

typedef struct
{
  uint32_t uptime_ms;
  uint16_t input_us;
  uint32_t input_period_us;
  uint16_t target_adc;
  uint16_t feedback_adc_raw;
  uint16_t feedback_adc;
  int16_t error_adc;
  int16_t motor_duty;
  uint8_t servo_state;
  uint8_t hbridge_state;
  uint8_t input_valid;
  uint8_t params_valid;
  uint32_t params_crc32;
  uint16_t adc_min_count;
  uint16_t adc_center_count;
  uint16_t adc_max_count;
  uint16_t input_age_ms;
  uint16_t vrefint_raw;
  uint16_t vdd_mv;
  uint16_t reserved;
} ServoTelemetry;

typedef struct
{
  uint8_t status;
  uint8_t command;
  uint16_t detail;
} ServoCommAck;

uint16_t Servo_Comm_Crc16(const uint8_t *data, size_t len);
bool Servo_Comm_BuildFrame(uint8_t command,
                           const uint8_t *payload,
                           uint8_t payload_len,
                           uint8_t *out_frame,
                           size_t out_capacity,
                           size_t *out_len);
uint8_t Servo_Comm_HandleFrame(const uint8_t *frame,
                               size_t frame_len,
                               uint8_t *response,
                               size_t response_capacity,
                               size_t *response_len);
void Servo_Comm_FillTelemetry(ServoTelemetry *telemetry);

#endif
