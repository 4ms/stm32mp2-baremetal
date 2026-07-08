# copro_m33_embedded - top-level Makefile
#
# Builds the Cortex-M33 firmware, embeds it into a C header, then builds the
# Cortex-A35 firmware which loads and starts the M33. Modeled on the
# copro_embedded example in the stm32mp1-baremetal repo.

export BINARYNAME = main
export BUILDDIR_M33 = build/corem33
export SHAREDDIR = ../shared

all: firmware_m33.h
	@$(MAKE) --no-print-directory -f makefile_a35.mk all

# Convert the M33 flat binary into a C array included by main_a35.cc. The array
# is made 'const' so it lands in .rodata (ROM), contiguous with .text. If it
# were left non-const it would go to .data at 0x90000000 (RAM) and, because this
# linkscript gives .data no separate load address, objcopy would pad the binary
# with the ~128MB gap between ROM and RAM.
firmware_m33.h: $(BUILDDIR_M33)/firmware.bin
	xxd -i -n m33_firmware $< | sed 's/^unsigned char/const unsigned char/' > $@

# Always ask the M33 sub-make to (re)build its binary.
$(BUILDDIR_M33)/firmware.bin: m33

m33:
	@$(MAKE) --no-print-directory -f makefile_m33.mk all

install: all
	@$(MAKE) --no-print-directory -f makefile_a35.mk install

clean:
	@$(MAKE) --no-print-directory -f makefile_m33.mk clean
	@$(MAKE) --no-print-directory -f makefile_a35.mk clean
	rm -f firmware_m33.h

.PHONY: all m33 install clean
