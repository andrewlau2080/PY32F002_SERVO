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

C_DEFS := -DUSE_HAL_DRIVER -DPY32F002Ax5

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
	src/servo_comm.c \
	src/app_hal_msp.c \
	src/app_it.c \
	$(PY32_SDK)/Templates/PY32F002xx_Templates/Src/system_py32f0xx.c \
	$(PY32_SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal.c \
	$(PY32_SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_rcc.c \
	$(PY32_SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_gpio.c \
	$(PY32_SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_adc.c \
	$(PY32_SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_tim.c \
	$(PY32_SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_tim_ex.c \
	$(PY32_SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_pwr.c \
	$(PY32_SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_flash.c \
	$(PY32_SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_cortex.c

ASM_SOURCES := \
	$(PY32_SDK)/Projects/PY32F002A-STK/Example/GPIO/GPIO_Toggle/EIDE/startup_py32f002xx.s

LDSCRIPT := ld/py32f002ax5_servo.ld

CFLAGS := $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -Wextra -ffunction-sections -fdata-sections -MMD -MP
ASFLAGS := $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -ffunction-sections -fdata-sections
LDFLAGS := $(MCU) -specs=nano.specs -T$(LDSCRIPT) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections
LIBS := -lc -lm -lnosys

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
