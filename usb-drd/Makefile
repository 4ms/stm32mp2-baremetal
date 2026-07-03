BINARYNAME = main
BUILDDIR = build
SHAREDDIR = ../shared

SOURCES := startup.s
SOURCES += system_init.cc
SOURCES += main.cc
SOURCES += mmu.cc
SOURCES += $(SHAREDDIR)/aarch64/vectors.S
SOURCES += $(SHAREDDIR)/drivers/hal_cnt.cc
SOURCES += $(SHAREDDIR)/drivers/pin.cc
SOURCES += $(SHAREDDIR)/interrupt/irq_init.c
SOURCES += $(SHAREDDIR)/interrupt/interrupt_handler.cc
SOURCES += $(SHAREDDIR)/newlib/syscall_stubs.c
SOURCES += $(SHAREDDIR)/newlib/syscall_stdio.c
SOURCES += $(SHAREDDIR)/newlib/sbrk.cc
SOURCES += $(SHAREDDIR)/newlib/libcpp_stub.cc
SOURCES += $(SHAREDDIR)/print/uart_print.c
SOURCES += $(SHAREDDIR)/print/print.cc
SOURCES += $(SHAREDDIR)/aarch64/esr_decode.cc
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_cortex.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_dma.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_dma_ex.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_rcc_ex.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_i2c.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_i2c_ex.c

SOURCES += dwc3_dma.c
SOURCES += dwc3_baremetal.c
SOURCES += cdc_acm.c

SOURCES_DWC := 
SOURCES_DWC += dwc3/core.c
SOURCES_DWC += dwc3/ep0.c
SOURCES_DWC += dwc3/gadget.c

SOURCES += $(SOURCES_DWC)

SOURCES_XHCI :=
SOURCES_XHCI += xhci/xhci.c
SOURCES_XHCI += xhci/xhci-ring.c
SOURCES_XHCI += xhci/xhci-mem.c

SOURCES += $(SOURCES_XHCI)
SOURCES += xhci_baremetal.c
SOURCES += usb_midi.c

# Board selection (run `make clean` when switching):
#   ev1      = STM32MP257 EV1 kit: TCPP03-M20 on I2C1 gates VBUS
#   devboard = custom board v0.1: CC pins direct to UCPD1, VBUS switch on
#              PB4 (adaptor board), VBUS sense on PH3
BOARD ?= ev1
ifeq ($(BOARD),ev1)
SOURCES += tcpp03.cc
SOURCES += typec_ev1.cc
else ifeq ($(BOARD),devboard)
SOURCES += typec_devboard.cc
BOARD_DEF := -DDEVBOARD_0_1
else
$(error Invalid BOARD '$(BOARD)' - must be ev1 or devboard)
endif
$(info Board: $(BOARD))

INCLUDES := -I.
INCLUDES += -I$(SHAREDDIR)
INCLUDES += -I$(SHAREDDIR)/cmsis/Include
INCLUDES += -I$(SHAREDDIR)/cmsis/Core_A/Include
INCLUDES += -I$(SHAREDDIR)/cmsis-device/Include

INCLUDES += -I$(SHAREDDIR)/STM32MP2xx_HAL_Driver/Inc

INCLUDES += -isystem u-boot/include
INCLUDES += -isystem u-boot/arch/arm/include
INCLUDES += -Iu-boot/include
INCLUDES += -Iu-boot/arch/arm/include

EXTRA_ARCH_CFLAGS := -DUSE_HAL_DRIVER

OPTFLAG := -O2

# Link stdlib for printf, malloc...
LINK_STDLIB := 

# Console UART: 1 = USART1 (PB8/PB10, AF6), 2 = ST-LINK (default), 6 = GPIO Expander
# header. See shared/makefile-common.mk.
UART_CHOICE := 6

# FPU := -ffast-math -funsafe-math-optimizations -mlow-precision-div -mlow-precision-sqrt -mlow-precision-recip-sqrt

FREESTANDING := 

$(BUILDDIR)/obj/obj/../shared/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_dma_ex.o: EXTRA_ARCH_CFLAGS += -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast 
$(BUILDDIR)/obj/obj/../shared/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_i2c.o: EXTRA_ARCH_CFLAGS += -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast

include $(SHAREDDIR)/makefile-common.mk


