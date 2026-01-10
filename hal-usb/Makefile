BINARYNAME = main
BUILDDIR = build
SHAREDDIR = ../shared

SOURCES := startup.s
SOURCES += vector_table.s
SOURCES += mmu.c
SOURCES += system_init.cc
SOURCES += main.cc
SOURCES += $(SHAREDDIR)/drivers/hal_cnt.cc
SOURCES += $(SHAREDDIR)/drivers/irq_init.c
SOURCES += $(SHAREDDIR)/drivers/interrupt_handler.cc
SOURCES += $(SHAREDDIR)/system/syscall_stubs.c
SOURCES += $(SHAREDDIR)/system/syscall_stdio.c
SOURCES += $(SHAREDDIR)/system/new.cc
SOURCES += $(SHAREDDIR)/uart_print.c
SOURCES += $(SHAREDDIR)/print.cc
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal.c
# SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_cortex.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_dma.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_pcd.c

INCLUDES := -I.
INCLUDES += -I$(SHAREDDIR)
INCLUDES += -I$(SHAREDDIR)/cmsis/Include
INCLUDES += -I$(SHAREDDIR)/cmsis/Core_A/Include
INCLUDES += -I$(SHAREDDIR)/cmsis-device/Include

INCLUDES += -I$(SHAREDDIR)/STM32MP2xx_HAL_Driver/Inc


EXTRA_ARCH_CFLAGS := -DA35_NON_SECURE \
					 -DUSE_HAL_DRIVER

OPTFLAG := -O0

include $(SHAREDDIR)/makefile-common.mk

