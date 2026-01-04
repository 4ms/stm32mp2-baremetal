BINARYNAME = main
BUILDDIR = build
SHAREDDIR = ../shared

SOURCES := startup.s
SOURCES += vector_table.s
SOURCES += mmu.c
SOURCES += system_init.c
SOURCES += print.c
SOURCES += main.cc
SOURCES += led.c
SOURCES += newlib_stub.c

INCLUDES := -I.

EXTRA_ARCH_CFLAGS := -DA35_NON_SECURE

OPTFLAG := -O0

include $(SHAREDDIR)/makefile-common.mk

