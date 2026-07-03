BINARYNAME = main
BUILDDIR = build
SHAREDDIR = ../shared

SOURCES := startup.s
SOURCES += main.cc
SOURCES += $(SHAREDDIR)/aarch64/vectors.S
SOURCES += $(SHAREDDIR)/mmu/mmu.cc
SOURCES += psci.cc
SOURCES += aux_core_startup.s
SOURCES += $(SHAREDDIR)/interrupt/irq_init.c
SOURCES += $(SHAREDDIR)/interrupt/interrupt_handler.cc
SOURCES += $(SHAREDDIR)/print/uart_print.c
SOURCES += $(SHAREDDIR)/print/print.cc
SOURCES += $(SHAREDDIR)/newlib/syscall_stubs.c
SOURCES += $(SHAREDDIR)/newlib/libcpp_stub.cc

INCLUDES := -I.
INCLUDES += -Iaarch64
INCLUDES += -I$(SHAREDDIR)
INCLUDES += -I$(SHAREDDIR)/cmsis/Include
INCLUDES += -I$(SHAREDDIR)/cmsis/Core_A/Include
INCLUDES += -I$(SHAREDDIR)/cmsis-device/Include

# Console UART: 1 = USART1 (PB8/PB10, AF6), 2 = ST-LINK (default), 6 = GPIO Expander
# header. See shared/makefile-common.mk. Uncomment to override, e.g.:
# UART_CHOICE := 6

OPTFLAG := -O0

include $(SHAREDDIR)/makefile-common.mk

