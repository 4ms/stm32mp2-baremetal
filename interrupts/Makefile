BINARYNAME = main
BUILDDIR = build
SHAREDDIR = ../shared

# Set MDRIVLIB and CPPUTIL to the location of these libraries and make sure
# mdrivlib/ is checked out to the stm32mp2 branch
MDRIVLIB ?= ../../mdrivlib
CPPUTIL ?= ../../cpputil

SOURCES := main.cc
SOURCES += $(MDRIVLIB)/target/stm32mp2_ca35/boot/startup_ca35.s
SOURCES += $(MDRIVLIB)/target/stm32mp2_ca35/boot/vectors.S
SOURCES += $(MDRIVLIB)/target/stm32mp2_ca35/boot/irq_init.c
SOURCES += $(MDRIVLIB)/target/stm32mp2_ca35/drivers/interrupt_handler.cc
SOURCES += $(MDRIVLIB)/target/stm32mp2_ca35/templates/mmu_ca35.cc
SOURCES += $(SHAREDDIR)/print/uart_print.c
SOURCES += $(SHAREDDIR)/print/print.cc
SOURCES += $(SHAREDDIR)/newlib/syscall_stubs.c
SOURCES += $(SHAREDDIR)/newlib/libcpp_stub.cc

INCLUDES := -I.
INCLUDES += -I$(MDRIVLIB)
INCLUDES += -I$(MDRIVLIB)/target/stm32mp2
INCLUDES += -I$(MDRIVLIB)/target/stm32mp2_ca35
INCLUDES += -I$(CPPUTIL)
INCLUDES += -I$(SHAREDDIR)
INCLUDES += -I$(SHAREDDIR)/cmsis/Include
INCLUDES += -I$(SHAREDDIR)/cmsis/Core_A/Include
INCLUDES += -I$(SHAREDDIR)/cmsis-device/Include

# Console UART: 1 = USART1 (PB8/PB10, AF6), 2 = ST-LINK (default), 6 = GPIO Expander
# header. See shared/makefile-common.mk. Uncomment to override, e.g.:
# UART_CHOICE := 6

OPTFLAG := -O0

include $(SHAREDDIR)/makefile-common.mk
