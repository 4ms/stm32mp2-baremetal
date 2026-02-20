BINARYNAME = main
BUILDDIR = build
SHAREDDIR = ../shared

SOURCES := startup.s
SOURCES += system_init.c
SOURCES += main.cc
SOURCES += led.c
SOURCES += $(SHAREDDIR)/system/syscall_stubs.c
SOURCES += $(SHAREDDIR)/mmu/mmu.cc
SOURCES += $(SHAREDDIR)/uart_print.c

INCLUDES := -I.

OPTFLAG := -O0

UART_CHOICE := 6

include $(SHAREDDIR)/makefile-common.mk

