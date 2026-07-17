BINARYNAME = main
BUILDDIR = build
SHAREDDIR = ../shared

SOURCES := startup.s
SOURCES += main.cc
# GPU library (etna API + 3D emitters)
SOURCES += ../gpu/etna.cc
SOURCES += ../gpu/etna_3d.cc
SOURCES += ../gpu/pmic.cc
# Display library (LTDC + LVDS + board wiring)
SOURCES += ../ltdc/display.cc
SOURCES += ../ltdc/ltdc.cc
SOURCES += ../ltdc/lvds.cc
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
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_i2c.c
SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_i2c_ex.c

INCLUDES := -I.
INCLUDES += -I../gpu
INCLUDES += -I../ltdc
INCLUDES += -I$(SHAREDDIR)
INCLUDES += -I$(SHAREDDIR)/cmsis/Include
INCLUDES += -I$(SHAREDDIR)/cmsis/Core_A/Include
INCLUDES += -I$(SHAREDDIR)/cmsis-device/Include
INCLUDES += -I$(SHAREDDIR)/STM32MP2xx_HAL_Driver/Inc

EXTRA_ARCH_CFLAGS := -DUSE_HAL_DRIVER

OPTFLAG := -O2

include $(SHAREDDIR)/makefile-common.mk
