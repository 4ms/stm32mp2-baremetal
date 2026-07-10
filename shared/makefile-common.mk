SDCARD_MOUNT_PATH ?= /Volumes/BAREAPP

LINKSCR ?= linkscript.ld
BUILDDIR ?= build
BINARYNAME ?= main
UIMAGENAME ?= $(BUILDDIR)/main.uimg
SCRIPTDIR ?= ../scripts

OBJDIR = $(BUILDDIR)/obj/obj
# Must match the ROM ORIGIN in the project's linker script. TF-A's baremetal
# loader and the uimg header both load/enter the image here.
LOADADDR 	?= 0x88000000
ENTRYPOINT 	?= 0x88000000

OBJECTS   = $(addprefix $(OBJDIR)/, $(addsuffix .o, $(basename $(SOURCES))))
DEPS   	  = $(addprefix $(OBJDIR)/, $(addsuffix .d, $(basename $(SOURCES))))


MCU := -mcpu=cortex-a35 
MCU += -march=armv8-a+fp+simd
MCU += -mtune=cortex-a35
MCU += -mlittle-endian

FPU ?=

EXTRA_ARCH_CFLAGS ?= 

# If you are hacking these projects to run in non-secure state,
# then you probably will want to change this to:
# EXTRA_ARCH_CFLAGS += -DA35_NON_SECURE
EXTRA_ARCH_CFLAGS += -DCORTEX_IN_SECURE_STATE 

# If you are hacking these projects to run at EL1, then you probably will want
# to comment this out:
EL_LEVEL ?= -DRUN_EL3


ARCH_CFLAGS ?= -DUSE_FULL_LL_DRIVER \
			   -DSTM32MP257Cxx \
			   -DSTM32MP2 \
			   -DCORE_CA35 \
			   $(EXTRA_ARCH_CFLAGS) \

OPTFLAG ?= -O0

# Console UART selection:
#   1 = USART1 on PB8 (TX) / PB10(RX), on the GPIO header
#   2 = USART2, connected to the ST-LINK adaptor via the USB-C jack (default)
#   6 = USART6 on the GPIO Expander header
# Default is EV1 board
BOARD ?= ev1

# If BOARD is `devboard` then default to using USART1 unless otherwise specifed.
# Print an error if an invalid board is specified
ifeq ($(BOARD),devboard)
  ifeq ($(origin UART),undefined)
    UART := 1
  endif
  BOARD_DEF := -DDEVBOARD_0_1
else ifeq ($(BOARD),ev1)
  BOARD_DEF := -DBOARD_EV1
else
  $(error Invalid BOARD '$(BOARD)' - must be devboard or ev1)
endif

# Default UART is ST-LINK
UART ?= 2

# Human-readable description per valid choice.
UART_1_DESC := USART1 (PB8/PB10 on custom board)
UART_2_DESC := USART2 (EV1 ST-LINK USB-C jack)
UART_6_DESC := USART6 (EV1 GPIO Expander header)
UART_DESC := $(UART_$(UART)_DESC)
ifeq ($(UART_DESC),)
$(error Invalid UART '$(UART)' - must be 1, 2, or 6)
endif

$(info Console UART: $(UART_DESC))

FREESTANDING ?= -ffreestanding

OPTION_FLAGS ?=
OPTION_FLAGS += \
		$(MCU) \
		${FREESTANDING} \
		$(EL_LEVEL) \
		-DUART=$(UART) \
		$(BOARD_DEF)


AFLAGS =  \
		-fdata-sections -ffunction-sections \
		-fno-common \
		-std=gnu99 \
		-nostdinc \
		-nostdlib \
		$(OPTION_FLAGS) 

		#-mstrict-align \

CFLAGS ?= -g2 \
		 -fno-common \
		 $(ARCH_CFLAGS) \
		 $(FPU) \
		 $(INCLUDES) \
		 -fdata-sections -ffunction-sections \
		 -nostartfiles \
		 $(OPTION_FLAGS) \
		 $(EXTRACFLAGS) \
		 -c 

CXXFLAGS ?= $(CFLAGS) \
		-std=c++23 \
		-fno-rtti \
		-fno-exceptions \
		-fno-unwind-tables \
		-fno-threadsafe-statics \
		-Werror=return-type \
		-Wdouble-promotion \
		-Wno-register \
		-Wno-volatile \
		$(EXTRACXXFLAGS) \

LINK_STDLIB ?= -nostdlib

LFLAGS ?= -Wl,--gc-sections \
		 -Wl,-Map,$(BUILDDIR)/$(BINARYNAME).map,--cref \
		 -T $(LINKSCR) \
		 $(LINK_STDLIB) \
		 $(MCU) \
		 -nostartfiles \
		 -Wl,--no-warn-rwx-segments \
		 $(EXTRALDFLAGS) \
		 ${FREESTANDING}
		 
# LFLAGS := --hash-style=gnu \
# 		  --as-needed \
		 # $(MCU) \

DEPFLAGS = -MMD -MP -MF $(OBJDIR)/$(basename $<).d

ARCH 	= aarch64-none-elf
CC 		= $(ARCH)-gcc
CXX 	= $(ARCH)-g++
LD 		= $(ARCH)-g++
AS 		= $(ARCH)-gcc
OBJCPY 	= $(ARCH)-objcopy
OBJDMP 	= $(ARCH)-objdump
GDB 	= $(ARCH)-gdb
SZ 		= $(ARCH)-size

SZOPTS 	= -d

ELF 	= $(BUILDDIR)/$(BINARYNAME).elf
HEX 	= $(BUILDDIR)/$(BINARYNAME).hex
BIN 	= $(BUILDDIR)/$(BINARYNAME).bin

all: Makefile $(ELF) $(BIN) $(UIMAGENAME)

# Force a rebuild if the build configuration (BOARD or UART selection)
# changes: objects don't otherwise depend on the -D defines these produce, so
# switching without a `make clean` would silently keep stale objects.
# Projects can append their own config vars to CONFIG_STAMP_VALS before
# including this file (e.g. usb-drd's USB_DEVICE_SPEED).
CONFIG_STAMP_VALS += $(BOARD)_uart$(UART)
CONFIG_STAMP = $(BUILDDIR)/config_is_$(subst $() ,_,$(strip $(CONFIG_STAMP_VALS)))
$(CONFIG_STAMP):
	@mkdir -p $(BUILDDIR)
	@rm -f $(BUILDDIR)/config_is_*
	@touch $@

$(OBJECTS): $(CONFIG_STAMP)

elf: $(ELF)

install:
	cp $(UIMAGENAME) $(SDCARD_MOUNT_PATH)
	diskutil unmount $(SDCARD_MOUNT_PATH)

openocd:
	openocd -s ../scripts/openocd -f board/stm32mp25x_dk.cfg

debug: $(ELF)
	$(ARCH)-gdb $(ELF)

$(OBJDIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(info Assembling $< at $(OPTFLAG))
	@$(AS) $(AFLAGS) -c $< -o $@ 

$(OBJDIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(info Assembling $< at $(OPTFLAG))
	@$(CC) $(AFLAGS) -c $< -o $@ 

$(OBJDIR)/%.o: %.c $(OBJDIR)/%.d
	@mkdir -p $(dir $@)
	$(info Building $< at $(OPTFLAG))
	@$(CC) $(DEPFLAGS) $(OPTFLAG) $(CFLAGS) $< -o $@

$(OBJDIR)/%.o: %.c[cp]* $(OBJDIR)/%.d
	@mkdir -p $(dir $@)
	$(info Building $< at $(OPTFLAG))
	@$(CXX) $(DEPFLAGS) $(OPTFLAG) $(CXXFLAGS) $< -o $@

$(ELF): $(OBJECTS) $(LINKSCR)
	$(info Linking...)
	@$(LD) $(LFLAGS) -o $@ $(OBJECTS) 

$(BIN): $(ELF)
	$(OBJCPY) -O binary $< $@

$(HEX): $(ELF)
	@$(OBJCPY) --output-target=ihex $< $@
	@$(SZ) $(SZOPTS) $(ELF)

$(UIMAGENAME): $(BIN)
	$(info Creating uimg file)
	python3 $(SCRIPTDIR)/uimg_header.py $< $@ $(LOADADDR) $(ENTRYPOINT)

# Copy the app to the SD card's "app" partition (TF-A boots it from there):
#   make flash SD=/dev/diskX
flash: $(UIMAGENAME)
ifndef SD
	$(error Usage: make flash SD=/dev/diskX  (the whole SD card device))
endif
	$(SCRIPTDIR)/flash-app.sh $(SD) $(UIMAGENAME)

.PHONY: flash

%.d: ;

clean:
	rm -rf build

ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPS)
endif

.PRECIOUS: $(DEPS) $(OBJECTS) $(ELF)
.PHONY: all clean install #install-mp1-boot

.PHONY: compile_commands
compile_commands:
	compiledb make
	compdb -p ./ list > compile_commands.tmp 2>/dev/null
	rm compile_commands.json
	mv compile_commands.tmp compile_commands.json
