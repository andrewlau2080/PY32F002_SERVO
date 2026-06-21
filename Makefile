TARGET := py32f002_servo
BUILD_DIR := build
PY32_SDK ?= /opt/py32/PY32F0xx_Firmware

PREFIX ?= arm-none-eabi-
CC := $(PREFIX)gcc
AS := $(PREFIX)gcc -x assembler-with-cpp
CP := $(PREFIX)objcopy
SZ := $(PREFIX)size

CPU := -mcpu=cortex-m0plus
MCU := $(CPU) -mthumb
OPT ?= -Os
LTO ?= 0
SERVO_ENABLE_TJC_LCDM ?= 0
SERVO_TJC_UART_ALT_PINS ?= 0
SERVO_TJC_RX_ENABLE ?=
SERVO_TJC_VISUAL_TEST ?=
SERVO_TJC_SOFT_TX ?=
TJC_LCDM_BAUD_OVERRIDE ?=
LCDM_TJC_CMD_GAP_MS_OVERRIDE ?=
LCDM_TJC_PAGE_CLEAR_MS_OVERRIDE ?=
LCDM_TJC_FONT_ID_OVERRIDE ?=
TJC_LCDM_VISUAL_TEST_MS_OVERRIDE ?=
BOARD_TSSOP20_DEBUG_PINS ?= 0
BOARD_DEBUG_ATTACH_WINDOW_MS ?=
SERVO_ENABLE_HBRIDGE_IO_TEST ?= 0
SERVO_ENABLE_ADC_LED_TEST ?= 0
SERVO_IO_TEST_ENABLE_GPIOB ?= 0
HBRIDGE_PWM_TICK_US_OVERRIDE ?=
SERVO_ENABLE_PA7_SQUARE_TEST ?= 0
SERVO_ENABLE_PA7_TJC_TEST ?= 0
SERVO_ENABLE_LCDM_RUNTIME_UI_TEST ?= 0
SERVO_ENABLE_INTERNAL_STEP_TEST ?= 0
SERVO_ALLOW_INTERNAL_STEP_TEST ?= 0
SERVO_ENABLE_PA0_POT_POWER ?= 0
SERVO_HBRIDGE_FORCE_HIZ ?= 0
SERVO_ENABLE_COMM ?= 1

C_DEFS := -DUSE_HAL_DRIVER -DPY32F002Ax5
ifeq ($(SERVO_ENABLE_TJC_LCDM),1)
C_DEFS += -DSERVO_ENABLE_TJC_LCDM=1
endif
ifeq ($(SERVO_TJC_UART_ALT_PINS),1)
C_DEFS += -DSERVO_TJC_UART_ALT_PINS=1
endif
ifneq ($(SERVO_TJC_RX_ENABLE),)
C_DEFS += -DSERVO_TJC_RX_ENABLE=$(SERVO_TJC_RX_ENABLE)
endif
ifneq ($(SERVO_TJC_VISUAL_TEST),)
C_DEFS += -DSERVO_TJC_VISUAL_TEST=$(SERVO_TJC_VISUAL_TEST)
endif
ifneq ($(SERVO_TJC_SOFT_TX),)
C_DEFS += -DSERVO_TJC_SOFT_TX=$(SERVO_TJC_SOFT_TX)
endif
ifneq ($(TJC_LCDM_BAUD_OVERRIDE),)
C_DEFS += -DTJC_LCDM_DEFAULT_BAUD=$(TJC_LCDM_BAUD_OVERRIDE)UL
C_DEFS += -DLCDM_TJC_BAUD=$(TJC_LCDM_BAUD_OVERRIDE)U
endif
ifneq ($(LCDM_TJC_CMD_GAP_MS_OVERRIDE),)
C_DEFS += -DLCDM_TJC_CMD_GAP_MS=$(LCDM_TJC_CMD_GAP_MS_OVERRIDE)U
endif
ifneq ($(LCDM_TJC_PAGE_CLEAR_MS_OVERRIDE),)
C_DEFS += -DLCDM_TJC_PAGE_CLEAR_MS=$(LCDM_TJC_PAGE_CLEAR_MS_OVERRIDE)U
endif
ifneq ($(LCDM_TJC_FONT_ID_OVERRIDE),)
C_DEFS += -DLCDM_TJC_FONT_ID=$(LCDM_TJC_FONT_ID_OVERRIDE)U
endif
ifneq ($(TJC_LCDM_VISUAL_TEST_MS_OVERRIDE),)
C_DEFS += -DTJC_LCDM_VISUAL_TEST_MS=$(TJC_LCDM_VISUAL_TEST_MS_OVERRIDE)U
endif
ifeq ($(BOARD_TSSOP20_DEBUG_PINS),1)
C_DEFS += -DBOARD_TSSOP20_DEBUG_PINS=1
endif
ifneq ($(BOARD_DEBUG_ATTACH_WINDOW_MS),)
C_DEFS += -DBOARD_DEBUG_ATTACH_WINDOW_MS=$(BOARD_DEBUG_ATTACH_WINDOW_MS)U
endif
ifeq ($(SERVO_ENABLE_HBRIDGE_IO_TEST),1)
C_DEFS += -DSERVO_ENABLE_HBRIDGE_IO_TEST=1
endif
ifeq ($(SERVO_ENABLE_ADC_LED_TEST),1)
C_DEFS += -DSERVO_ENABLE_ADC_LED_TEST=1
endif
ifeq ($(SERVO_IO_TEST_ENABLE_GPIOB),1)
C_DEFS += -DSERVO_IO_TEST_ENABLE_GPIOB=1
endif
ifneq ($(HBRIDGE_PWM_TICK_US_OVERRIDE),)
C_DEFS += -DHBRIDGE_PWM_TICK_US=$(HBRIDGE_PWM_TICK_US_OVERRIDE)U
endif
ifeq ($(SERVO_ENABLE_PA7_SQUARE_TEST),1)
C_DEFS += -DSERVO_ENABLE_PA7_SQUARE_TEST=1
endif
ifeq ($(SERVO_ENABLE_PA7_TJC_TEST),1)
C_DEFS += -DSERVO_ENABLE_PA7_TJC_TEST=1
endif
ifeq ($(SERVO_ENABLE_LCDM_RUNTIME_UI_TEST),1)
C_DEFS += -DSERVO_ENABLE_LCDM_RUNTIME_UI_TEST=1
endif
ifeq ($(SERVO_ALLOW_INTERNAL_STEP_TEST),1)
ifeq ($(SERVO_ENABLE_INTERNAL_STEP_TEST),1)
C_DEFS += -DSERVO_ENABLE_INTERNAL_STEP_TEST=1
endif
endif
ifeq ($(SERVO_ENABLE_PA0_POT_POWER),1)
C_DEFS += -DSERVO_ENABLE_PA0_POT_POWER=1
endif
ifeq ($(SERVO_HBRIDGE_FORCE_HIZ),1)
C_DEFS += -DSERVO_HBRIDGE_FORCE_HIZ=1
endif

C_INCLUDES := \
	-Iinc \
	-I$(PY32_SDK)/Drivers/PY32F0xx_HAL_Driver/Inc \
	-I$(PY32_SDK)/Drivers/CMSIS/Device/PY32F0xx/Include \
	-I$(PY32_SDK)/Drivers/CMSIS/Include

C_SOURCES := \
	src/app_entry.c \
	src/board.c \
	src/adc_feedback.c \
	src/pwm_input.c \
	src/hbridge.c \
	src/servo_control.c \
	src/servo_params.c \
	src/app_hal_msp.c \
	src/app_it.c \
	$(PY32_SDK)/Templates/PY32F002xx_Templates/Src/system_py32f0xx.c \
	$(PY32_SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal.c \
	$(PY32_SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_rcc.c \
	$(PY32_SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_gpio.c \
	$(PY32_SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_adc.c \
	$(PY32_SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_pwr.c \
	$(PY32_SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_flash.c \
	$(PY32_SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_cortex.c

ifeq ($(SERVO_ENABLE_COMM),1)
C_SOURCES += \
	src/servo_comm.c
endif

ifeq ($(SERVO_ENABLE_TJC_LCDM),1)
C_SOURCES += \
	src/tjc_lcdm.c
endif

ifneq ($(filter 1,$(SERVO_ENABLE_TJC_LCDM) $(SERVO_ENABLE_LCDM_RUNTIME_UI_TEST)),)
C_SOURCES += \
	$(PY32_SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_uart.c
endif

ASM_SOURCES := \
	$(PY32_SDK)/Projects/PY32F002A-STK/Example/GPIO/GPIO_Toggle/EIDE/startup_py32f002xx.s

LDSCRIPT := ld/py32f002ax5_servo.ld

CFLAGS := $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -Wextra -ffunction-sections -fdata-sections -MMD -MP
ASFLAGS := $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -ffunction-sections -fdata-sections
LDFLAGS := $(MCU) -nostartfiles -specs=nano.specs -T$(LDSCRIPT) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections
LIBS := -lc -lm -lnosys

ifeq ($(LTO),1)
CFLAGS += -flto
ASFLAGS += -flto
LDFLAGS += -flto
endif

OBJECTS := $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

.PHONY: all clean size flash

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) $(LIBS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(CP) -O ihex $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(CP) -O binary -S $< $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

flash: $(BUILD_DIR)/$(TARGET).hex
	/opt/py32-venv/bin/pyocd flash -t py32f002ax5 $<

-include $(wildcard $(BUILD_DIR)/*.d)
