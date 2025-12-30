# stm32mp2-baremetal

This contains works-in-progress exploring using the STM32MP2xx chips in a
bare-metal context on all cores.

The goal is to run full applications with access:
  - Clock configuration to run at full speed (1.5GHz for CA35, 400MHz for CM33)
  - Nested interrupts (using the GIC)
  - USB dual-role host/device, leveraging the STM32 USB library
  - SAI (audio), 
     - Using DMA
     - 2 SAI instances, each capable of 8in/8out at 96kHz and block size of 16 frames
  - RGB or MIPI/DSI video
  - I2C
  - ADC running with DMA
  - Multi-core operation:
     - Startup code for each core (main CA35, aux CA35, CM33)
     - IPCC and HSEM
     - core-specific interrupts (SGI)

# Project setup

Go ahead and clone all the repos now:

```bash
mkdir mp2-dev    #this will be our project directory
cd mp2-dev

git clone https://github.com/4ms/tf-a-stm32mp25.git
git clone https://github.com/4ms/u-boot-stm32mp25
git clone https://github.com/4ms/stm32mp2-baremetal
```

Now you should have a directory structure like this:

```
mp2-dev/
   |_ tf-a-stm32mp25/
   |_ u-boot-stm32mp25/
   |_ stm32mp2-baremetal/
```

We'll add some more build dirs to the mp2-dev dir as we go along.

# Baremetal Application

We will skip the topic of bootloaders so we can get started more quickly. To do this, we'll use
the stock SD Card supplied by ST (or flash your own card using their Starter Package or Developer Package
instructions on their wiki)

For building your own bootloader, skip down to `Bootloaders`


## Building a baremetal application

```bash
cd stm32mp2-baremetal
cd minimal_boot  #pick a project
make

# View the output file:
ls -l build/main.uimg
```

## Copying the baremetal application to an SD Card:

Using the stock SD Card from ST, you first need to format partition 11 as FATFS
(it's in ext4 by default). Doing so is OS-specific.

Then, copy the `main.uimg` file onto the SD Card partition 11 using your OS (not dd).

```bash
cp build/main.uimg /Volumes/sd-part11/
```

## Running a baremetal application without custom FIP or U-Boot

You can boot with the stock SD Card image provided by ST, and then halt U-Boot
when it asks "Hit any key..." 

Then type in the boot command:

```
mmc dev 0
fatload mmc 0:b 0x88000000 main.uimg
go 0x88000000
```

This of course requires partition 11 to be already formatted as FATFS and that
you already copied main.uimg to it (see previous step).

## Debugging

1. The Discovery and Eval boards have an SWD connection via the USB jack marked "ST-LINK". 
Connect that to your computer.

2. Power on with the SD card installed, and hit a key to stop U-boot from doing anything.

3. Start openocd. The scripts for the stm32mp2 chips are included in this repo, as well as 
the openocd config file (`openocd.cfg`). So start openocd in a new terminal window like this:
```bash
# From stm32mp2-baremetal/, which is where the openocd.cfg file lives:
openocd
```

You should see:

```
Open On-Chip Debugger 0.12.0
Licensed under GNU GPL v2
For bug reports, read
	http://openocd.org/doc/doxygen/bugs.html
srst_only srst_pulls_trst srst_gates_jtag srst_open_drain connect_deassert_srst

Info : Listening on port 6666 for tcl connections
Info : Listening on port 4444 for telnet connections
Info : STLINK V3J12M3B5S1 (API v3) VID:PID 0483:3753
Info : Target voltage: 3.287408
Info : Unable to match requested speed 5000 kHz, using 3300 kHz
Info : Unable to match requested speed 5000 kHz, using 3300 kHz
Info : clock speed 3300 kHz
Info : stlink_dap_op_connect(connect)
Info : SWD DPIDR 0x6ba02477
Info : stm32mp25x.a35_0: hardware has 6 breakpoints, 4 watchpoints
Info : stm32mp25x.a35_1: hardware has 6 breakpoints, 4 watchpoints
Info : [stm32mp25x.m33] Cortex-M33 r1p0 processor detected
Info : [stm32mp25x.m33] target has 8 breakpoints, 4 watchpoints
Info : [stm32mp25x.m0p] Cortex-M0+ r0p1 processor detected
Info : [stm32mp25x.m0p] target has 4 breakpoints, 2 watchpoints
Info : gdb port disabled
Info : gdb port disabled
Info : gdb port disabled
Info : gdb port disabled
Info : starting gdb server for stm32mp25x.a35_0 on 3333
Info : Listening on port 3333 for gdb connections
Info : starting gdb server for stm32mp25x.m33 on 3334
Info : Listening on port 3334 for gdb connections
Info : starting gdb server for stm32mp25x.m0p on 3335
Info : Listening on port 3335 for gdb connections
Info : [stm32mp25x.m33] external reset detected
Info : [stm32mp25x.m0p] external reset detected`
```

4. Connect via gdb in another terminal window:

```bash
aarch64-none-elf-gdb build/main.elf
```

If gdb complains that it has a security setting preventing it from running the .gdbinit script, then
either follow the instructions it provides to allow this directory, or run the commands yourself:

```
target extended-remote:3333
thbreak *0x88000040
```

You can then do normal gdb stuff to load the image and debug it.

```
load build/main.elf
file build/main.elf
list main
continue
si
x/64i 0x88000040
```

### Support files:

The SVD files are here:
https://github.com/STMicroelectronics/meta-st-stm32mp/tree/scarthgap/recipes-devtools/cmsis-svd/cmsis-svd


OpenOCD stuff here:
https://github.com/STMicroelectronics/meta-st-stm32mp/tree/scarthgap/recipes-devtools/openocd


The CA35 CMSIS device headers are missing from the STM32MP2 Cube HAL (only the M33 and M0 are present).
But they are here in the DDR Firmware repo:
https://github.com/STMicroelectronics/STM32DDRFW-UTIL/tree/main/Drivers/CMSIS/Device/ST/STM32MP2xx/Include


# Bootloaders:

__Note: you can skip this entire Bootloaders section and use the stock SD Card from ST. (see above)__

**Boot stages**
- Boot Loader stage 1 (BL1) _AP Trusted ROM_ == BOOTROM on MP2
- Boot Loader stage 2 (BL2) _Trusted Boot Firmware_ runs at EL3 == TF-A
- Boot Loader stage 3-1 (BL31) _EL3 Runtime Software_ aka Secure Monitor. SYSRAM -- TODO...
- Boot Loader stage 3-2 (BL32) _Secure-EL1 Payload_ = OPTEE or maybe SP_min -- TODO...
- Boot Loader stage 3-3 (BL33) _Non-trusted Firmware_ == U-boot


We ultimately build a FIP that integrates all the required bootloaders into one file.
Then we flash that fip file to an SD Card (or some other media like internal flash or eMMC).
The bootloaders setup the system and then finally load our application from an SD Card (or 
some other media, but we'll assume SD Card since it's most accessible).


First, let's clone the bootloader repos:

```bash
cd ..   # go to the overall project dir, the parent dir of stm32mp2-baremetal/
git clone https://github.com/4ms/tf-a-stm32mp25.git
git clone https://github.com/4ms/u-boot-stm32mp25
```

Now you should have a directory structure like this:

```
mp2-dev/
   |_ tf-a-stm32mp25/
   |_ u-boot-stm32mp25/
   |_ stm32mp2-baremetal/
```


## TF-A

Read the docs: https://trustedfirmware-a.readthedocs.io/en/v2.11/plat/st/stm32mp2.html

TF-A is the BL2 bootloader.

To build you need the aarch64-none-elf-gcc toolchain. Versions 12.3 and 13.1 have been tested but
probably any later version will also work (please open an issue if you find a version that doesn't).

Set the path to it like this (notice the trailing dash `-`)

```
export CROSS_COMPILE=/path/to/arm-gnu-toolchain-12.3.rel1-darwin-arm64-aarch64-none-elf/bin/aarch64-none-elf-
```


Build TF-A like this:

```bash
cd tf-a-stm32mp25
make PLAT=stm32mp2 DTB_FILE_NAME=stm32mp257f-ev1.dtb STM32MP_SDMMC=1 SPD=opteed STM32MP_DDR4_TYPE=1

# See the files we built:
ls -l build/stm32mp2/release/tf-a-stm32mp257f-ev1.stm32
ls -l build/stm32mp2/release/bl31.bin
ls -l build/stm32mp2/release/bl2.bin
```

TODO: I haven't quite finished this process yet, so for now we don't use these files! But we still need TF-A 
in order to build and use fiptool.


## U-boot

U-boot loads to 0x84000000, max end 0x84400000


Requires openssl 1.1.1 (does not work with 3.x)

On macOS, install like this:
```bash
brew install openssl@1.1
```


This also requires the `CROSS_COMPILE` variable to be set (See TF-A section above).

To build (adjust the paths to the openssl installation):

```bash
export LDFLAGS="-L/opt/homebrew/opt/openssl@1.1/lib"
export CPPFLAGS="-I/opt/homebrew/opt/openssl@1.1/include"
export PKG_CONFIG_PATH="/opt/homebrew/opt/openssl@1.1/lib/pkgconfig"

cd u-boot-stm32mp25

mkdir -p ../build-baremetal-uboot
make stm32mp25_baremetal_defconfig  O=../build-baremetal-uboot
make all O=../build-baremetal-uboot

# The u-boot binary we built:
ls -l build-baremetal-uboot/u-boot-notdb.bin
```

## Building OP-TEE

SKIP THIS FOR NOW


TODO: See https://wiki.st.com/stm32mpu/wiki/How_to_build_OP-TEE_components

Eventually once we figure out OP-TEE, we'll combine the TF-A, U-Boot, and OP-TEE binaries like this:

```bash
make PLAT=stm32mp2 DTB_FILE_NAME=stm32mp257f-ev1.dtb STM32MP_SDMMC=1 SPD=opteed STM32MP_DDR4_TYPE=1 \
    BL32=../optee-stm32mp25/build/bl32.bin BL33=../build-baremetal-uboot/u-boot.bin fip
```


## Using an existing FIP so we don't have to build OP-TEE

I haven't gotten OP-TEE to build yet, so for now we use an existing fip file
provided by ST, and replace the U-boot component with our own.

Download the STM32MP2 developer package, and copy one of the fip files:

```bash
cp stm32mp25-openstlinux-6.1-yocto-mickledore-mp2-v23.12.06/images/stm32mp25/fip/fip-stm32mp257f-ev1-ca35tdcid-ostl-m33-examples-ddr.bin fip.bin
```

Build fiptool, which is located in the TF-A project:

```bash
cd tf-a-stm32mp25
make fiptool

# Verify it built:
./tools/fiptool/fiptool help
```

Now use fiptool to replace the U-boot binary:

```bash
cd .. # run from the mp2-dev project root
tf-a-stm32mp25/tools/fiptool/fiptool --verbose update --nt-fw build-baremetal-uboot/u-boot-nodtb.bin fip.bin
```

You should see this output (size might vary):
```
DEBUG: Adding image build-baremetal-uboot/u-boot-nodtb.bin
DEBUG: Metadata size: 96 bytes
DEBUG: Payload size: 1418400 bytes
```

The `fip.bin` file now contains our custom-built U-boot.

## Installing the bootloader FIP onto an SD Card

TODO: how to partition and format the disk.

For now, use the SD Card that came with the EV1 board.

Copy the fip.bin to partition 5 of the sd card:

```bash
sudo dd if=baremetal-1/fip.bin of=/dev/diskXs5 
```

If you need to clear the U-boot environment variables, you can clear partition 7:

```bash
sudo dd if=/dev/zero of=/dev/diskXs7
```

