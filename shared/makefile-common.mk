SDCARD_MOUNT_PATH ?= /Volumes/BAREAPP

LINKSCR ?= linkscript.ld
# EXTLIBDIR ?= ../../third-party
# UBOOTDIR ?= $(EXTLIBDIR)/u-boot/build
BUILDDIR ?= build
BINARYNAME ?= main
UIMAGENAME ?= $(BUILDDIR)/main.uimg
SCRIPTDIR ?= ../scripts

OBJDIR = $(BUILDDIR)/obj/obj
LOADADDR 	?= 0x88000000
ENTRYPOINT 	?= 0x88000000

OBJECTS   = $(addprefix $(OBJDIR)/, $(addsuffix .o, $(basename $(SOURCES))))
DEPS   	  = $(addprefix $(OBJDIR)/, $(addsuffix .d, $(basename $(SOURCES))))


MCU := -mcpu=cortex-a35 
MCU += -march=armv8-a
MCU += -mtune=cortex-a35
MCU += -mlittle-endian

# FIXME: unrecognized by aarch64-none-elf-g++:
# FPU := -mfpu=neon
# FPU += -mfloat-abi=hard

EXTRA_ARCH_CFLAGS ?= 

ARCH_CFLAGS ?= -DUSE_FULL_LL_DRIVER \
			   -DSTM32MP257Cxx \
			   -DSTM32MP2 \
			   -DCORE_CA35 \
			   $(EXTRA_ARCH_CFLAGS) \

OPTFLAG ?= -O0

AFLAGS =  \
	-fdata-sections -ffunction-sections \
	-fno-builtin \
	-fno-common \
	-march=armv8-a \
	-mgeneral-regs-only \
	-mstrict-align \
	-std=gnu99 \
	-nostdinc \
	-nostdlib \
	-ffreestanding \

CFLAGS ?= -g2 \
		 -fno-common \
		 $(ARCH_CFLAGS) \
		 $(MCU) \
		 $(FPU) \
		 $(INCLUDES) \
		 -fdata-sections -ffunction-sections \
		 -nostartfiles \
		 -ffreestanding \
		 $(EXTRACFLAGS)\
		 -c \

CXXFLAGS ?= $(CFLAGS) \
		-std=c++20 \
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
		 $(EXTRALDFLAGS) \
		 -ffreestanding \
		 
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

elf: $(ELF)

install:
	cp $(UIMAGENAME) $(SDCARD_MOUNT_PATH)
	diskutil unmount $(SDCARD_MOUNT_PATH)

openocd:
	openocd -s ../scripts/openocd -f board/stm32mp25x_dk.cfg

debug: $(ELF)
	$(ARCH)-gdb $(ELF)

# install-mp1-boot:
# 	@if [ "$${SD_DISK_DEVPART}" = "" ]; then echo "Please specify the disk and partition like this: make install-mp1-boot SD_DISK_DEVPART=/dev/diskXs3"; \
# 	else \
# 	echo "sudo dd if=${UIMAGENAME} of=$${SD_DISK_DEVPART}" && \
# 	sudo dd if=${UIMAGENAME} of=$${SD_DISK_DEVPART};  fi

$(OBJDIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(info Building $< at $(OPTFLAG))
	$(AS) $(AFLAGS) -c $< -o $@ 

$(OBJDIR)/%.o: %.c $(OBJDIR)/%.d
	@mkdir -p $(dir $@)
	$(info Building $< at $(OPTFLAG))
	$(CC) $(DEPFLAGS) $(OPTFLAG) $(CFLAGS) $< -o $@

$(OBJDIR)/%.o: %.c[cp]* $(OBJDIR)/%.d
	@mkdir -p $(dir $@)
	$(info Building $< at $(OPTFLAG))
	$(CXX) $(DEPFLAGS) $(OPTFLAG) $(CXXFLAGS) $< -o $@

$(ELF): $(OBJECTS) $(LINKSCR)
	$(info Linking...)
	$(LD) $(LFLAGS) -o $@ $(OBJECTS) 

$(BIN): $(ELF)
	$(OBJCPY) -O binary $< $@

$(HEX): $(ELF)
	@$(OBJCPY) --output-target=ihex $< $@
	@$(SZ) $(SZOPTS) $(ELF)

$(UIMAGENAME): $(BIN)
	$(info Creating uimg file)
	python3 $(SCRIPTDIR)/uimg_header.py $< $@ $(LOADADDR) $(ENTRYPOINT)

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
