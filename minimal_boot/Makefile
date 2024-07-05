BINARYNAME = main
BUILDDIR = build
SHAREDDIR = ../shared

SOURCES := minboot.s
# SOURCES := $(SHARED)/system/syscalls.c
# SOURCES := $(SHARED)/system/libc_stub.c
# SORUCES += main.cc

INCLUDES := -I.
INCLUDES += -I$(SHAREDDIR)/STM32MP2xx_HAL_Driver/Inc
INCLUDES += -I$(SHAREDDIR)/CMSIS/Include

include $(SHAREDDIR)/makefile-common.mk

