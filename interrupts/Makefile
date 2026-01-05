BINARYNAME = main
BUILDDIR = build
SHAREDDIR = ../shared

SOURCES := startup.s
SOURCES += vector_table.s
SOURCES += mmu.c
SOURCES += system_init.c
SOURCES += main.cc
SOURCES += led.c
SOURCES += newlib_stub.c
SOURCES += uart_print.c
SOURCES += $(SHAREDDIR)/print.cc

INCLUDES := -I.
INCLUDES += -I$(SHAREDDIR)
INCLUDES += -I$(SHAREDDIR)/cmsis/Include
INCLUDES += -I$(SHAREDDIR)/cmsis/Core_A/Include
INCLUDES += -I$(SHAREDDIR)/cmsis-device/Include

EXTRA_ARCH_CFLAGS := -DA35_NON_SECURE

OPTFLAG := -O0

include $(SHAREDDIR)/makefile-common.mk

