BINARYNAME = main
BUILDDIR = build
SHAREDDIR = ../shared

SOURCES := startup.s
SOURCES += main.cc
SOURCES += display.cc
SOURCES += ltdc.cc
SOURCES += nv3052c.cc
SOURCES += $(SHAREDDIR)/aarch64/vectors.S
SOURCES += $(SHAREDDIR)/mmu/mmu.cc
SOURCES += $(SHAREDDIR)/drivers/hal_cnt.cc
SOURCES += $(SHAREDDIR)/interrupt/irq_init.c
SOURCES += $(SHAREDDIR)/interrupt/interrupt_handler.cc
SOURCES += $(SHAREDDIR)/print/uart_print.c
SOURCES += $(SHAREDDIR)/print/print.cc
SOURCES += $(SHAREDDIR)/aarch64/esr_decode.cc
SOURCES += $(SHAREDDIR)/newlib/syscall_stubs.c
SOURCES += $(SHAREDDIR)/newlib/libcpp_stub.cc
SOURCES += $(SHAREDDIR)/drivers/pin.cc
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_ltdc.c

INCLUDES := -I.
INCLUDES += -I$(SHAREDDIR)
INCLUDES += -I$(SHAREDDIR)/cmsis/Include
INCLUDES += -I$(SHAREDDIR)/cmsis/Core_A/Include
INCLUDES += -I$(SHAREDDIR)/cmsis-device/Include
INCLUDES += -I$(SHAREDDIR)/STM32MP2xx_HAL_Driver/Inc

EXTRA_ARCH_CFLAGS := -DUSE_HAL_DRIVER

# The parallel-RGB header (ER-TFT3.95-1 panel) only exists on the custom
# devboard, so build for it by default: this selects the USART1 console
# (PB8/PB10) and defines DEVBOARD_0_1. (See shared/makefile-common.mk.)
BOARD ?= devboard

OPTFLAG := -O2

include $(SHAREDDIR)/makefile-common.mk
