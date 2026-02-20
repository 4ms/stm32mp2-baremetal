BINARYNAME = main
BUILDDIR = build
SHAREDDIR = ../shared

SOURCES := start.S
SOURCES += main.cc
SOURCES += led.c
SOURCES += $(SHAREDDIR)/newlib/syscall_stubs.c
SOURCES += $(SHAREDDIR)/print/uart_print.c

INCLUDES := -I.

OPTFLAG := -O0

UART_CHOICE := 6

include $(SHAREDDIR)/makefile-common.mk

