BINARYNAME = main
BUILDDIR = build
SHAREDDIR = ../shared

SOURCES := startup.s
SOURCES += main.cc
SOURCES += $(SHAREDDIR)/aarch64/vectors.S
SOURCES += $(SHAREDDIR)/mmu/mmu.cc
SOURCES += $(SHAREDDIR)/drivers/risab.cc
SOURCES += $(SHAREDDIR)/drivers/pin.cc
SOURCES += $(SHAREDDIR)/drivers/hal_cnt.cc
SOURCES += $(SHAREDDIR)/interrupt/irq_init.c
SOURCES += $(SHAREDDIR)/interrupt/interrupt_handler.cc
SOURCES += $(SHAREDDIR)/print/uart_print.c
SOURCES += $(SHAREDDIR)/print/print.cc
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_cortex.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_adc.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_adc_ex.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_dma.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_dma_ex.c
SOURCES += $(SHAREDDIR)/aarch64/esr_decode.cc
SOURCES += $(SHAREDDIR)/newlib/syscall_stubs.c
SOURCES += $(SHAREDDIR)/newlib/libcpp_stub.cc

INCLUDES := -I.
INCLUDES += -I$(SHAREDDIR)
INCLUDES += -I$(SHAREDDIR)/cmsis/Include
INCLUDES += -I$(SHAREDDIR)/cmsis/Core_A/Include
INCLUDES += -I$(SHAREDDIR)/cmsis-device/Include

INCLUDES += -I$(SHAREDDIR)/STM32MP2xx_HAL_Driver/Inc

EXTRA_ARCH_CFLAGS := -DUSE_HAL_DRIVER

OPTFLAG := -O2

# Board selection (run `make clean` when switching):
#   devboard = custom board v0.1: VREF+ pin is unloaded, so the internal
#              VREFBUF drives it (1.5V). Console on USART1.
#   ev1      = STM32MP257 EV1 kit: VREF+ pin is wired to the 1.8V rail, so
#              the VREFBUF is left in external-reference mode (it must not
#              drive the pin). Console on ST-LINK.
# A command-line UART_CHOICE=n wins over the per-board default.
BOARD ?= devboard
ifeq ($(BOARD),devboard)
UART_CHOICE := 1
else ifeq ($(BOARD),ev1)
BOARD_DEF := -DBOARD_EV1
else
$(error Invalid BOARD '$(BOARD)' - must be devboard or ev1)
endif
$(info Board: $(BOARD))

include $(SHAREDDIR)/makefile-common.mk

$(BUILDDIR)/obj/obj/../shared/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_dma_ex.o: EXTRA_ARCH_CFLAGS += -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast 
$(BUILDDIR)/obj/obj/../shared/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_adc.o: EXTRA_ARCH_CFLAGS += -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast 
