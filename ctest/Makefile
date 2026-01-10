BINARYNAME = main
BUILDDIR = build
SHAREDDIR = ../shared

SOURCES := startup.s
SOURCES += mmu.c
SOURCES += system_init.c
SOURCES += main.cc
SOURCES += led.c
SOURCES += $(SHAREDDIR)/system/syscall_stubs.c
SOURCES += $(SHAREDDIR)/uart_print.c

INCLUDES := -I.

OPTFLAG := -O0

include $(SHAREDDIR)/makefile-common.mk

