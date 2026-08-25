BINARYNAME = main
BUILDDIR = build
SHAREDDIR = ../shared

SOURCES := startup.s
SOURCES += main.cc
# GPU library (etna API + 3D emitters)
SOURCES += ../gpu/etna.cc
SOURCES += ../gpu/etna_3d.cc
SOURCES += ../gpu/etna_compute.cc
SOURCES += ../gpu/pmic.cc
SOURCES += ../gpu/gpu_mmuv2.cc

BOARD ?= ev1
ifeq ($(BOARD),devboard)
  DISPLAY ?= dsi
else
  DISPLAY ?= lvds
endif

ifeq ($(DISPLAY),lvds)
  DISPLAYDIR := ../ltdc-lvds
  SOURCES += $(DISPLAYDIR)/lvds.cc
  DISPLAY_DEF := -DDISPLAY_LVDS
else ifeq ($(DISPLAY),dsi)
  DISPLAYDIR := ../ltdc-dsi
  SOURCES += $(DISPLAYDIR)/dsi.cc
  SOURCES += $(DISPLAYDIR)/ili9881c.cc
  # ltdc-dsi drives the DSI + LTDC through ST's HAL drivers
  SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_dsi.c
  SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_ltdc.c
  DISPLAY_DEF := -DDISPLAY_DSI
else ifeq ($(DISPLAY),rgb)
  DISPLAYDIR := ../ltdc-rgb
  SOURCES += $(DISPLAYDIR)/nv3052c.cc
  SOURCES += $(SHAREDDIR)/STM32MP2xx_HAL_Driver/Src/stm32mp2xx_hal_ltdc.c
  # ltdc-rgb uses RGB565, the GPU resolves ARGB8888
  DISPLAY_DEF := -DDISPLAY_RGB -DLTDC_RGB_BYTES_PER_PIXEL=4
else
  $(error Invalid DISPLAY '$(DISPLAY)' - must be lvds, dsi or rgb)
endif
$(info Display: $(DISPLAY) ($(DISPLAYDIR)))
SOURCES += $(DISPLAYDIR)/display.cc
SOURCES += $(DISPLAYDIR)/ltdc.cc
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
INCLUDES += -I$(DISPLAYDIR)
INCLUDES += -I$(SHAREDDIR)
INCLUDES += -I$(SHAREDDIR)/cmsis/Include
INCLUDES += -I$(SHAREDDIR)/cmsis/Core_A/Include
INCLUDES += -I$(SHAREDDIR)/cmsis-device/Include
INCLUDES += -I$(SHAREDDIR)/STM32MP2xx_HAL_Driver/Inc

EXTRA_ARCH_CFLAGS := -DUSE_HAL_DRIVER $(DISPLAY_DEF)


DDRPERF ?= n
ifeq ($(DDRPERF),1)
$(info Will display DDR Performance monitor after first 120 frames)
SOURCES += ../gpu/perfmon.cc
EXTRA_ARCH_CFLAGS += -DDDRPERF=1
endif


OPTFLAG := -O2

include $(SHAREDDIR)/makefile-common.mk
