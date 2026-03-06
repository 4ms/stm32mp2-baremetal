BINARYNAME = main
BUILDDIR = build
SHAREDDIR = ../shared

SOURCES := startup.s
SOURCES += system_init.cc
SOURCES += main.cc
SOURCES += mmu.cc
SOURCES += dwc3_dma.c
SOURCES += $(SHAREDDIR)/aarch64/vectors.S
SOURCES += $(SHAREDDIR)/drivers/hal_cnt.cc
SOURCES += $(SHAREDDIR)/drivers/pin.cc
SOURCES += $(SHAREDDIR)/interrupt/irq_init.c
SOURCES += $(SHAREDDIR)/interrupt/interrupt_handler.cc
SOURCES += $(SHAREDDIR)/newlib/syscall_stubs.c
SOURCES += $(SHAREDDIR)/newlib/libcpp_stub.cc
SOURCES += $(SHAREDDIR)/print/uart_print.c
SOURCES += $(SHAREDDIR)/print/print.cc
SOURCES += $(SHAREDDIR)/aarch64/esr_decode.cc
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_cortex.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_dma.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_dma_ex.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_rcc_ex.c

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

# Uncomment this to use USART6 via the GPIO Expander header
# UART_CHOICE := 6

OPTFLAG := -Ofast 
FPU := -ffast-math -funsafe-math-optimizations -mlow-precision-div -mlow-precision-sqrt -mlow-precision-recip-sqrt

FREESTANDING := 

$(BUILDDIR)/obj/obj/../shared/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_dma_ex.o: EXTRA_ARCH_CFLAGS += -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast 

include $(SHAREDDIR)/makefile-common.mk


