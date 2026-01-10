BINARYNAME = main
BUILDDIR = build
SHAREDDIR = ../shared

SOURCES := startup.s
SOURCES += vector_table.s
SOURCES += mmu.c
SOURCES += system_init.cc
SOURCES += main.cc
SOURCES += usb_serial_device.cc

SOURCES += $(SHAREDDIR)/drivers/hal_cnt.cc
SOURCES += $(SHAREDDIR)/drivers/irq_init.c
SOURCES += $(SHAREDDIR)/drivers/interrupt_handler.cc
SOURCES += $(SHAREDDIR)/system/syscall_stubs.c
SOURCES += $(SHAREDDIR)/system/syscall_stdio.c
SOURCES += $(SHAREDDIR)/system/sbrk.cc
SOURCES += $(SHAREDDIR)/uart_print.c
SOURCES += $(SHAREDDIR)/print.cc
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_dma.c

SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_pcd.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_ll_usb_drd.c

SOURCES += stm32-usb-device-lib/Core/Src/usbd_core.c
SOURCES += stm32-usb-device-lib/Core/Src/usbd_ctlreq.c
SOURCES += stm32-usb-device-lib/Core/Src/usbd_ioreq.c

SOURCES += stm32-usb-device-lib/Class/CDC/Src/usbd_cdc.c

INCLUDES := -I.
INCLUDES += -I$(SHAREDDIR)
INCLUDES += -I$(SHAREDDIR)/cmsis/Include
INCLUDES += -I$(SHAREDDIR)/cmsis/Core_A/Include
INCLUDES += -I$(SHAREDDIR)/cmsis-device/Include
INCLUDES += -Istm32-usb-device-lib/Core/Inc
INCLUDES += -Istm32-usb-device-lib/Class/CDC/Inc

INCLUDES += -I$(SHAREDDIR)/STM32MP2xx_HAL_Driver/Inc

# not freestanding
FREESTANDING :=

EXTRA_ARCH_CFLAGS := -DA35_NON_SECURE \
					 -DUSE_HAL_DRIVER

OPTFLAG := -O0

include $(SHAREDDIR)/makefile-common.mk

