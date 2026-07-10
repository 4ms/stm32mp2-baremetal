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

# Console UART: 1 = USART1 (PB8/PB10, AF6), 2 = ST-LINK (default), 6 = GPIO Expander
# header. See shared/makefile-common.mk. Uncomment to override, e.g.:
# UART := 6

include $(SHAREDDIR)/makefile-common.mk

