BINARYNAME = main
BUILDDIR = build
SHAREDDIR = ../shared

SOURCES := startup.s
SOURCES += vector_table.s
SOURCES += mmu.c
SOURCES += system_init.cc
SOURCES += main.cc
SOURCES += led.c
SOURCES += psci.cc
SOURCES += aarch64/drivers/irq_init.c
SOURCES += aarch64/drivers/interrupt_handler.cc
SOURCES += $(SHAREDDIR)/system/libc_stub.c
SOURCES += $(SHAREDDIR)/uart_print.c
SOURCES += $(SHAREDDIR)/print.cc

INCLUDES := -I.
INCLUDES += -Iaarch64
INCLUDES += -I$(SHAREDDIR)
INCLUDES += -I$(SHAREDDIR)/cmsis/Include
INCLUDES += -I$(SHAREDDIR)/cmsis/Core_A/Include
INCLUDES += -I$(SHAREDDIR)/cmsis-device/Include

EXTRA_ARCH_CFLAGS := -DA35_NON_SECURE

OPTFLAG := -O0

include $(SHAREDDIR)/makefile-common.mk

