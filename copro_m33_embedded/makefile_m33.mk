# Builds the Cortex-M33 coprocessor firmware and produces a flat binary
# (build/corem33/firmware.bin) that the A35 build embeds. Standalone: unlike the
# A35 build it targets arm-none-eabi / Cortex-M33, so it does not include the
# aarch64 shared/makefile-common.mk.

BUILDDIR  = build/corem33
SHAREDDIR = ../shared
BINARYNAME = main

ARCH   = arm-none-eabi
CC     = $(ARCH)-gcc
CXX    = $(ARCH)-g++
LD     = $(ARCH)-g++
OBJCPY = $(ARCH)-objcopy
SZ     = $(ARCH)-size

LINKSCR = linkscript_m33.ld
OPTFLAG = -O0

MCU = -mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mthumb -mfloat-abi=hard -mlittle-endian

UART_CHOICE ?= 2
DEFS = -DUART=$(UART_CHOICE)

# Needed for stm32mp2xx_ll_gpio:
DEFS += -DUSE_FULL_LL_DRIVER \
			   -DSTM32MP257Cxx \
			   -DSTM32MP2 \
			   -DCORE_CM33

# Objects don't otherwise depend on UART_CHOICE, so changing it would leave a
# stale uart_print.o (compiled for the old UART) unless we force a rebuild.
# Record the current choice in a stamp file all objects depend on.
UART_STAMP = $(BUILDDIR)/uart_choice_is_$(UART_CHOICE)
$(UART_STAMP):
	@mkdir -p $(BUILDDIR)
	@rm -f $(BUILDDIR)/uart_choice_is_*
	@touch $@

INCLUDES = -I. -I$(SHAREDDIR)

# Needed for stm32mp2xx_ll_gpio:
INCLUDES += -I$(SHAREDDIR)/STM32MP2xx_HAL_Driver/Inc
INCLUDES += -I$(SHAREDDIR)/cmsis-device/Include
INCLUDES += -I$(SHAREDDIR)/cmsis/Include

SOURCES  = startup_m33.s
SOURCES += main_m33.cc
SOURCES += $(SHAREDDIR)/print/print.cc
SOURCES += $(SHAREDDIR)/print/uart_print.c
SOURCES += $(SHAREDDIR)/drivers/pin.cc

CFLAGS = -g2 -fno-common $(MCU) $(DEFS) $(INCLUDES) \
	-fdata-sections -ffunction-sections -ffreestanding -nostdlib -nostartfiles

CXXFLAGS = $(CFLAGS) -std=c++23 -fno-rtti -fno-exceptions -fno-unwind-tables \
	-fno-threadsafe-statics -Werror=return-type -Wno-register -Wno-volatile

LFLAGS = -Wl,--gc-sections -Wl,-Map,$(BUILDDIR)/$(BINARYNAME).map,--cref \
	$(MCU) -T $(LINKSCR) -nostdlib -nostartfiles -Wl,--no-warn-rwx-segments

OBJDIR  = $(BUILDDIR)/obj
OBJECTS = $(addprefix $(OBJDIR)/, $(addsuffix .o, $(basename $(SOURCES))))

$(OBJECTS): $(UART_STAMP)

ELF = $(BUILDDIR)/$(BINARYNAME).elf
BIN = $(BUILDDIR)/firmware.bin

all: $(BIN)

$(OBJDIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(info M33: Assembling $<)
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(info M33: Building $<)
	@$(CXX) $(OPTFLAG) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(info M33: Building $<)
	@$(CC) $(OPTFLAG) $(CFLAGS) -c $< -o $@

$(ELF): $(OBJECTS) $(LINKSCR)
	$(info M33: Linking $@)
	@$(LD) $(LFLAGS) -o $@ $(OBJECTS)
	@$(SZ) $(ELF)

# Flat binary the A35 embeds: vectors + text + rodata + data, contiguous from
# 0x0A060000 (.bss and stack are NOLOAD, so they are not included).
$(BIN): $(ELF)
	@$(OBJCPY) -O binary $< $@
	@ls -l $@

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean
