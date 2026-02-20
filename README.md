# stm32mp2-baremetal

This contains works-in-progress exploring using the STM32MP2xx chips in a
bare-metal context on all cores.

I have only had access to an STM32MP257F-EV1 board, so all these examples
are designed to run on that board.

The goal is to run full applications with access:
  - √ EL3 Secure state
  - √ Nested interrupts (using the GIC)
  - √ MMU configuration
  - √ DMA memory-to-memory transfers
  - √ Clock configuration to run at full speed (1.5GHz for CA35, 400MHz for CM33)
  - √ SAI (audio)
     - √ Using DMA
     - √ SAI TX (DAC audio out)
     - √ SAI RX (ADC audio in)
  - √ I2C
  - SMP Multi-core (dual CA35):
     - √ Startup code for each core
     - √ core-specific interrupts (SGI)
     - IPCC and HSEM for sharing data 
  - AMP multi-core (CM33)
  - AMP multi-core (CM0+)
  - USB dual-role host/device, leveraging the STM32 USB library
     - Device via USB3DR
     - Host via EHCI USBH (WIP partially working)
  - RGB or MIPI/DSI video
  - ADC running with DMA

# Project setup

Clone the repo into a parent directory, which will help keep things organized if you do any
work with U-boot, TF-A, or OP-TEE:

```bash
mkdir mp2-dev    # The project parent dir
cd mp2-dev

git clone https://github.com/4ms/stm32mp2-baremetal
cd stm32mp2-baremetal
```

Now you should have a directory structure like this:

```
mp2-dev/
   |_ stm32mp2-baremetal/
```

We'll add some more build dirs to the mp2-dev dir as we go along.

# WIP!!!

This branch (el3) is a work in progress to convert the examples to work in EL3 secure. In some cases, support for
running in EL1 non-secure states will continue, though I do not intend to full test the examples for that situation.

To run in EL3, a modified TF-A BL2 (FSBL) is used. This FSBL is greatly
simplified (much easier to build), and essentially loads your application as a
secure payload to DDR RAM. There is no OP-TEE, U-Boot, or BL31 Secure Monitor requirements anymore.
You only need the TF-A BL2 FSBL and your application.

See [our tf-a-stm32mp25 fork](https://github.com/4ms/tf-a-stm32mp25). The
`BAREMETAL_IMAGE_LOADER` build flag tells TF-A to run our app in EL3 secure
state.


| Project | EL3 (S) | EL1 (NS) | Notes |
| --- | :---: | :---: | --- |
| minimal_boot | √ | √ | |
| ctest | √ | √ | Drops to EL1 in startup |
| hal-dma | √ | ? | Not tested in EL1 |
| hal-audio | no  | √ | |
| i2c | no  | √ | |
| interrupts | no  | √ | |
| multicore_smp | no  | √ | |
| watchdog | no  | √ | TF-A has watchdog disabled, so project will not be converted |


# Baremetal Application

We will skip the topic of bootloaders for now so we can get started more quickly.
We'll use the stock SD card supplied by ST (or flash your own card using
their [Starter
Package](https://www.st.com/en/embedded-software/stm32mp2starter.html) or
[Developer Package](https://www.st.com/en/embedded-software/stm32mp2dev.html)
following instructions on the [ST
wiki](https://wiki.st.com/stm32mpu/wiki/Getting_started/STM32MP2_boards/STM32MP257x-EV1/Let%27s_start/Populate_the_target_and_boot_the_image)
)

With the stock SD card, you can run only some of the projects:
- minimal_boot
- watchdog
- interrupts
- multicore_smp

In order to run the more advanced projects that use the peripherals, you need
to have a custom build of OP-TEE which gives security permissions. Skip down to
`Bootloaders` for instructions how to do this.


## Building a baremetal application

If you've already installed our build of OP-TEE your SD Card, then you can try any project.
Otherwise, if you're using the stock card from ST, then the projects mentioned
above will work. The other projects use various peripherals which ST's OP-TEE
does not allow to be accessed by a non-secure app.

```bash
cd stm32mp2-baremetal
cd minimal_boot
make

# View the output file:
ls -l build/main.uimg
```


## Running a baremetal application via U-Boot

To load an application with U-Boot, you need to copy the uimg file to
the SD card.

Using the stock SD card from ST, you first need to format partition 11 as FATFS
(it's in ext4 by default). Doing so is OS-specific. 

*Note: If you are using a Linux host, you could leave partition 11 as ext4, and
change the `fatload` command below to ext4load, but FAT is easier for Mac and
Windows to use, so we'll be using that in this tutorial.*

Then, copy the `main.uimg` file onto the SD Card partition 11 using your OS (not dd).

```bash
cp build/main.uimg /Volumes/sd-part11/
```


Connect your computer via USB to the USB jack marked "ST-LINK" on the EV1 board.

Power on with the SD card installed, and open a console terminal on your
computer to the TTY port that the USB connection provides. The baud rate is
115200 8N1. For example, using minicom on macOS (the device number varies): 

```bash
minicom -D /dev/cu.usbmodem1102
```

Halt U-Boot when it asks "Hit any key..." by pressing a key.

Then at the U-BOOT prompt, type in the boot command:

```
mmc dev 0
fatload mmc 0:b 0x88000040 main.uimg
bootm 0x88000040
```

This of course requires partition 11 to be already formatted as FATFS and that
you already copied main.uimg to it (see previous step).

You can also save this command in U-boot's environment variables, so that you don't have to 
type it each time. In fact, if you save it in `bootcmd` then it'll run automatically without
you needing to press a key:

```
env set bootcmd "mmc dev 0;fatload mmc 0:b 0x88000040 main.uimg;bootm 0x88000040"
env save
```

You can run a command with `run`:
```
run bootcmd
```

If you ever need to wipe the U-boot environment variables, you can clear partition 7.
Use dd (from your computer, not from the U-Boot prompt):

```bash
sudo dd if=/dev/zero of=/dev/diskX7
```


## Loading an app via SWD (and how to debug)

1. The Discovery and Eval boards have an SWD connection via the USB jack marked
   "ST-LINK" which you are already using for the console terminial.

3. Power on, and watch the console for "Hit any key...". Then hit a key to
   stop U-boot from continuing. You should be at the U-Boot prompt now.

3. Start openocd. The scripts for the stm32mp2 chips are included in this repo,
   as well as the openocd config file (`openocd.cfg`). So start openocd in a
   new terminal window like this:

```bash
cd stm32mp2-baremetal     # must be in this dir where the openocd.cfg file lives
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
cd [example project dir]
make
aarch64-none-elf-gdb build/main.elf
```

If gdb complains that it has a security setting preventing it from running the
.gdbinit script, then either follow the instructions it provides to allow this
directory, or run the command contained in the .gdbinit file yourself:

```
target extended-remote:3333
```

You can then do normal gdb stuff to load the image and debug it.

```
load build/main.elf
thbreak main
disassemble
list main
continue
si
n
x/64i 0x88000040
```

### Resetting

If you re-compile and need to load the new binary, unfortunately the best way
I've found is to press the hard reset button on the EVK board. Usually openocd
will re-connect, but if it doesn't then you have to unplug/plug the USB cable
so it can re-enumerate the ST-LINK USB device. After doing this, you need to
quit gdb and re-start it. This process is could use improvement (especially
losing the gdb history), so if anyone has a better way, please let me know
(open an issue, PR, or comment). I've tried various commands like `monitor
halt` and `monitor reset` but they inevitably make it harder to connect.


### Support files:

The SVD files are here:
https://github.com/STMicroelectronics/meta-st-stm32mp/tree/scarthgap/recipes-devtools/cmsis-svd/cmsis-svd


OpenOCD stuff here:
https://github.com/STMicroelectronics/meta-st-stm32mp/tree/scarthgap/recipes-devtools/openocd


The CA35 CMSIS device headers are missing from the STM32MP2 Cube HAL (only the M33 and M0 are present).
But they are here in the DDR Firmware repo:
https://github.com/STMicroelectronics/STM32DDRFW-UTIL/tree/main/Drivers/CMSIS/Device/ST/STM32MP2xx/Include
I've copied them into this repo in the shared/cmsis-device dir.

There is a serious issue in the stm32mp2xx.h file provided by ST, that breaks
several HAL peripherals. The issue is the POSITION_VAL function macro does not
work on 64-bit systems. This has been corrected in the version in this repo.

# Replacing the OP-TEE binaries

In order to run all the projects, you need a custom version of OP-TEE loaded.
The fastest way to run all the example projects is to replace OP-TEE with our custom build on your SD card.

1. Insert your SD card into your computer

2. In the terminal run this command to copy the original FIP file to your computer. Replace /dev/diskX5 with the device name for partition 5.
 ``` bash
 sudo dd if=/dev/diskX5 of=fip-original.bin
 ```

3. Build TF-A fiptool:
```bash
cd ..   # go to the overall project dir, the parent dir of stm32mp2-baremetal/
git clone https://github.com/4ms/tf-a-stm32mp25.git
cd tf-a-stm32mp25
make fiptool PLAT=stm32mp2

# on MacOS you may need to do this instead:
make fiptool PLAT=stm32mp2 OPENSSL_DIR=/opt/homebrew/opt/openssl@1.1 HOSTCCFLAGS="-I/opt/homebrew/opt/openssl@1.1/include"

# Check fiptool works:
tools/fiptool/fiptool help
```

4. Download pre-built `tee-header_v2.bin` and `tee-pager_v2.bin` OP-TEE binaries from [here](https://github.com/4ms/optee-stm32mp25/releases)

5. Install the new OP-TEE binaries onto your fip file:
```bash
cp fip-original.bin fip-modified.bin
tf-a-stm32mp25/tools/fiptool/fiptool --verbose update \
    --tos-fw /path/to/tee-header_v2.bin \
    --tos-fw-extra1 /path/to/tee-pager_v2.bin fip-modified.bin
```

6. Install the modified fip file back onto the SD card (use the same partition as step 2)
 ``` bash
 sudo dd if=fip-modified.bin of=/dev/diskX5 
 ```

7. Boot the EV1 board as normal via the SD card, halting at the U-Boot prompt. Load your example project as described above.



# Bootloaders In Depth:


## Overview 

**Boot stages**
- Boot Loader stage 1 (BL1) _AP Trusted ROM_ == BOOTROM
- Boot Loader stage 2 (BL2) _Trusted Boot Firmware_ == TF-A
- Boot Loader stage 3-1 (BL31) _EL3 Runtime Software_ == Secure Monitor, which also is TF-A
- Boot Loader stage 3-2 (BL32) _Secure-EL1 Payload_ == OPTEE (or SP_min or none)
- Boot Loader stage 3-3 (BL33) _Non-trusted Firmware_ == U-boot


The bootloaders setup the system and then finally load our application
from an SD card (or some other media, but we'll assume SD card since it's most
accessible).

All these bootloaders plus their support files are combined together into a FIP file.
Then we flash that FIP file to a particular partition of the SD card.

We only need to replace OP-TEE, since that contains security settings that severly limit
what peripherals our application has access to.

The easiest thing to do is to use ST's stock SD card, read the FIP file off the card,
then replace only the OP-TEE binary in the FIP file, and finally write the modified FIP
back to the card. Details on how to do this are below.

## Setup

First, let's clone the bootloader repos:

```bash
cd ..   # go to the overall project dir, the parent dir of stm32mp2-baremetal/
git clone https://github.com/4ms/tf-a-stm32mp25.git
git clone https://github.com/4ms/u-boot-stm32mp25
git clone https://github.com/4ms/optee-stm32mp25.git
```

Now you should have a directory structure like this:

```
mp2-dev/
   |_ optee-stm32mp25/
   |_ stm32mp2-baremetal/
   |_ tf-a-stm32mp25/
   |_ u-boot-stm32mp25/
```


## TF-A

Read the docs: https://trustedfirmware-a.readthedocs.io/en/v2.11/plat/st/stm32mp2.html

TF-A is the BL2 bootloader. This mostly just loads the fip file into the correct places 
in memory and also starts some of the images executing. 

TF-A also contains the BL31 "Secure Monitor". The Secure Monitor runs
in the background while your app runs, handling some system calls via interrupts.

TF-A also contains the fiptool, which is required for modifying the FIP binary. 

To build you need the aarch64-none-elf-gcc toolchain. Versions 12.3, 13.1, and 14.2 have been tested but
probably any later version will also work (please open an issue if you find a version that doesn't).

Set the path to it like this (notice the trailing dash `-`)

```bash
export PATH=/path/to/arm-gnu-toolchain-12.3.rel1-darwin-arm64-aarch64-none-elf/bin/:$PATH
```


Build TF-A like this:

```bash
cd tf-a-stm32mp25
make PLAT=stm32mp2 DTB_FILE_NAME=stm32mp257f-ev1.dtb STM32MP_SDMMC=1 SPD=opteed STM32MP_DDR4_TYPE=1 CROSS_COMPILE=aarch64-none-elf-

# See the files we built:
ls -l build/stm32mp2/release/tf-a-stm32mp257f-ev1.stm32
ls -l build/stm32mp2/release/bl31.bin
ls -l build/stm32mp2/release/bl2.bin
```

On MacOS, you may need to build like this:
```bash
make PLAT=stm32mp2 DTB_FILE_NAME=stm32mp257f-ev1.dtb STM32MP_SDMMC=1 SPD=opteed STM32MP_DDR4_TYPE=1 \
    OPENSSL_DIR=/opt/homebrew/opt/openssl@1.1 HOSTCCFLAGS="-I/opt/homebrew/opt/openssl@1.1/include"
```


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
export HOSTCFLAGS="-I/opt/homebrew/opt/openssl@1.1/include"
export PKG_CONFIG_PATH="/opt/homebrew/opt/openssl@1.1/lib/pkgconfig"

cd u-boot-stm32mp25

mkdir -p ../build-baremetal-uboot
make stm32mp25_baremetal_defconfig  O=../build-baremetal-uboot
make all O=../build-baremetal-uboot

# The u-boot binary we built:
ls -l build-baremetal-uboot/u-boot-notdb.bin
```

## Building OP-TEE

### Download pre-built binaries

If you just want to run the example projects, you can use [our build of OP-TEE here](https://github.com/4ms/optee-stm32mp25/releases)

### Building on your own

To build, first you will need the python prerequisites:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install cffi crytpography pyelftools pillow
```

You may also need to install the python package `cffi` (TODO: confirm).

```bash
cd optee-stm32mp25

# Make sure you're on the right branch
git checkout mp257-ev1-baremetal

# Make sure the arm-none-eabi-gcc and aarch64-none-elf-gcc toolchains are on your PATH:
# These versions are known to work, but other later versions probably work too.
export PATH=/path/to/arm-gnu-toolchain-12.3.rel1-darwin-arm64-aarch64-none-elf/bin/:$PATH
export PATH=/path/to/arm-gnu-toolchain-13.2.rel1-darwin-arm64-arm-none-eabi/bin/:$PATH

# Build
make -j4    PLATFORM=stm32mp2  PLATFORM_FLAVOR=257F_EV1 ARCH=arm ARM64_core=y \
            CFG_EMBED_DTB_SOURCE_FILE=stm32mp257f-ev1.dts \
            CROSS_COMPILE64=aarch64-none-elf- \
            CROSS_COMPILE=arm-none-eabi- \
            CFG_TEE_CORE_LOG_LEVEL=4 \
            O=build
```

The `mp257-ev1-baremetal` branch is based on ST's v3.19.0-stm32mp branch. I was
only able to get v3.19.0 to work, not the latest v4.0.0 from ST's fork.

The only modifications is to open up more access to various peripherals in the
device tree. The git log is the best way to see all the changes.

### Giving your app access to peripherals

OP-TEE is responsible for setting up the RIFSC peripheral which grants permission
to access certain peripherals from the non-secure world (i.e. your app).
By default, many peripherals are not given permission to the non-secure world.

If you try to use one of
them in your app, you'll get a RIF error: the console will display an error message
that includes an ID number and will freeze.

The ID number can be used to determine which peripheral access caused the fault.
Table 38 of the Reference Manual lists these IDs. For example, if you try to run
the hal-audio example with the stock OP-TEE, you'll get a RIF error with ID 50.
Looking at Table 38, we see that SAI2 is ID 50, and indeed SAI2 is the audio peripheral
used in that example (or maybe you see 147 for the HPDMA, or 137 for the DDR/SRAM).

The solution is to modify the dtsi file in OP-TEE.
Typically this is in the `&rifsc` section of `core/arch/arm/dts/stm32mp257f-ev1-ca35tdcid-rif.dtsi`. 
Then re-compile, install the op-tee binaries into the FIP, and put that onto the SD card.


For example, in this line which controls RCC resource 108 (MCO1):
```dtsi
&rifsc {
	st,protreg = <
        //...
        RIFPROT(RIF_RCC_RESOURCE(108), RIF_UNUSED, RIF_UNLOCK, RIF_SEC, RIF_PRIV, RIF_CID1, RIF_SEM_DIS, RIF_CFEN)
```

I changed the `RIF_SEC` to `RIF_NSEC` to give the app permission to configure the MCO1
peripheral via the MCO1CFGR register. The `RIF_SEC` flag refers to Secure world,
but we only have access to the non-secure world
from our app, so we have to change it to NSEC (non-secure) to use it.

I don't believe changing RIF_PRIV to RIF_NPRIV has an effect since we are
operating in priviledged status (EL1), but I'm not positive. `RIF_CFDIS` seems
like the most open setting, though `RIF_CFEN` in combination with
`RIF_CID1` also seems to work since that means the RIFSC will filter for CID1
(which is what out app is running on).

### Giving your app access to clocks 

In addition to peripherals, you might need to give your app access to clocks.

For example:
- MCO1 has a setting for the configuration (MCO1CFGR) and another access
  setting for the clock (CK_MCO1). 

- The SAI peripherals have a kernel clock that must be opened up in the `&rcc`
  section, in addition to peripheral access setting in the `&rifsc` section. 

I'm still learning all the details of the RIFSC peripheral, so consult the
Reference Manual if you need to give your app access to something. 

Table 133 of the Reference Manual lists all the kernel clocks. This table 
tells you the FLEXGEN Channel Number for a particular kernel clock. For 
example SAI2 has the ck_ker_sai2 clock which is channel number 24.
This number is the same as the RCC Resource ID. 

If your clock is not on this table, then look at Table 183 to find the RCC
resource ID. For example, PLL7 has the ID 64.

Knowing the RCC Resource ID, you can check if the clock already has non-secure
permissions by looking at the corresponding bit in `RCC->SECCFGR[]`, or check
the settings in `RCC->R[x].CIDCFGR`. Or you can just change the line in the 
OP-TEE rif dtsi file in the `&rcc` section. For example:

```dts
&rcc {
    st,protreg = <
    //...
    RIFPROT(RIF_RCC_RESOURCE(24), RIF_UNUSED, RIF_UNLOCK, RIF_NSEC, RIF_NPRIV, RIF_CID1, RIF_SEM_DIS, RIF_CFEN) /* CK_KER_SAI2 */

```

This line controls the RIFSC setings for SAI2's kernel clock. To set this clock's
speed you will need to set the XBAR (aka Flexbar) configuration
Usually this means choosing one of the PLL's from PLL4 to PLL7 and setting an integer
division amount. In order to have access to the XBAR configuration registers, you will
need to tell OP-TEE to give permission to the non-secure world to access this.

This clock is not to be confused with the SAI peripheral clock, which is APB2. The SAI
peripheral runs off the bus clock, but also needs an additional clock (the kernel clock)
to derive the audio sample rate.

### Modifying the dtsi file

ST's CubeMX software will generate the rif dtsi file for optee, though it
doesn't let you check/uncheck all boxes so it's impossible to create certain
useful and valid settings. But it does generate comments in the that helps
decipher some of the cryptic IDs.

I've made manual modifications in a somewhat haphhazard way, just giving access
where needed for my own purposes. In some cases I wasn't sure what the correct
setting was, as you can see if you look at the git log for our optee fork.

For a real application, you should review the security settings and
be more intentional about the choices.

After making modifications to the dtsi file, make sure to rebuild. The dtsi and dts
files are compiled into the op-tee binary (as opposed to Linux where the dts is
compiled into a dtb and included seperately from the kernel).


## Creating a FIP

Once you have all the bootloaders compiled, you can create a FIP file.

We will use TF-A to create the FIP, since it has a built in make target for doing so.

```bash
cd tf-a-stm32mp25

make ARCH=aarch64 PLAT=stm32mp2 \
    SPD=opteed \
    STM32MP_DDR4_TYPE=1 \
    STM32MP_SDMMC=1 \
    DTB_FILE_NAME=stm32mp257f-ev1.dtb \
    BL32=../optee-stm32mp25/out/arm-plat-stm32mp2/core/tee-header_v2.bin \
    BL32_EXTRA1=../optee-stm32mp25/out/arm-plat-stm32mp2/core/tee-pager_v2.bin \
    BL33=../build-baremetal-uboot/u-boot-nodtb.bin \
    BL33_CFG=../build-baremetal-uboot/u-boot.dtb \
    fip

ls -l build/release/stm32mp2/fip.bin
```

TF-A also has the `fiptool` command line tool, which you can use instead of the above.

Build fiptool, which is located in the TF-A project:

```bash
cd tf-a-stm32mp25
make fiptool PLAT=stm32mp2

# Verify it built:
./tools/fiptool/fiptool help
```

On macOS, you might need to change `make fiptool` to this:
```
make fiptool PLAT=stm32mp2 OPENSSL_DIR=/opt/homebrew/opt/openssl@1.1 HOSTCCFLAGS="-I/opt/homebrew/opt/openssl@1.1/include"
```


To use fiptool to create a FIP file, instead of TF-A's make target, you would do this:
```bash
cd tf-a-stm32mp25

tools/fiptool/fiptool create \
    --ddr-fw drivers/st/ddr/phy/firmware/bin/stm32mp2/ddr4_pmu_train.bin \
    --soc-fw-config build/stm32mp2/release/fdts/stm32mp257f-ev1-bl31.dtb \
    --soc-fw build/stm32mp2/release/bl31.bin \
    --fw-config build/stm32mp2/release/fdts/stm32mp257f-ev1-fw-config.dtb \
    --tos-fw ../optee-stm32mp25/out/arm-plat-stm32mp2/core/tee-header_v2.bin \
    --tos-fw-extra1 ../optee-stm32mp25/out/arm-plat-stm32mp2/core/tee-pager_v2.bin \
    --nt-fw ../build-baremetal-uboot/u-boot-nodtb.bin \
    --hw-config ../build-baremetal-uboot/u-boot.dtb \
    build/stm32mp2/release/fip.bin

ls -l build/release/stm32mp2/fip.bin
```


## Using an existing FIP so we don't have to build all the bootloaders

Instead of building everything from scratch, you could use an existing fip file
provided by ST, and replace one or more components with one you built.
This method was outlined briefly above.

Download the STM32MP2 developer package, and copy one of the fip files:

```bash
cp stm32mp25-openstlinux-6.1-yocto-mickledore-mp2-v23.12.06/images/stm32mp25/fip/fip-stm32mp257f-ev1-ca35tdcid-ostl-m33-examples-ddr.bin fip.bin
```

Or copy the existing fip file off your SD card:

```bash
sudo dd if=/dev/diskX5 of=fip.bin
```

Now use fiptool to replace just one binary. For example, this will replace the U-boot binary:

```bash
cd .. # run from the mp2-dev project root
tf-a-stm32mp25/tools/fiptool/fiptool --verbose update --nt-fw build-baremetal-uboot/u-boot-nodtb.bin fip.bin
```

Or this will replace the OP-TEE binary:
```bash
cd .. # run from the mp2-dev project root
tf-a-stm32mp25/tools/fiptool/fiptool --verbose update \
    --tos-fw ../optee-stm32mp25/out/core/tee-header_v2.bin \
    --tos-fw-extra1 ../optee-stm32mp25/out/core/tee-pager_v2.bin fip.bin
```

The `fip.bin` file will be updated.

Copy the fip.bin to partition 5 of the SD card:

```bash
sudo dd if=baremetal-1/fip.bin of=/dev/diskXs5 
```

Insert the card into the EV board and power cycle.

# Misc Info

## SD Card info

From TF-A boot, the SD card is organized:
```
Partition table with 8 entries:
1: fsbla1                              4400-443fc
2: fsbla2                              44400-843fc
3: metadata1                           84400-c43fc
4: metadata2                           c4400-1043fc
5: fip-a                               104400-5043fc
6: fip-b                               504400-9043fc
7: u-boot-env                          904400-9843fc
8: bootfs                              984400-49843fc
```

## FIP info


 Example output from fiptool (the offsets are not valid)
```
EL3 Runtime Firmware BL31: offset=0x178, size=0x114BD, cmdline="--soc-fw"
Secure Payload BL32 (Trusted OS): offset=0x11635, size=0x1C, cmdline="--tos-fw"
Secure Payload BL32 Extra1 (Trusted OS Extra1): offset=0x11651, size=0xD0788, cmdline="--tos-fw-extra1"
Non-Trusted Firmware BL33: offset=0xE1DD9, size=0x15A4A0, cmdline="--nt-fw"
FW_CONFIG: offset=0x23C279, size=0x326, cmdline="--fw-config"
HW_CONFIG: offset=0x23C59F, size=0x17290, cmdline="--hw-config"
SOC_FW_CONFIG: offset=0x25382F, size=0x39FF, cmdline="--soc-fw-config"
DDR_FW: offset=0x25722E, size=0x7524, cmdline="--ddr-fw"
```

TF-A BL2 loads images from the FIP in this order:
(The image ids are from plat/st/stm32mp2/include/plat_tbbr_img_def.h)
(Note that the sizes don't match above because this info was collected from different builds)

- Image id 26 (DDR_FW_ID): DDR firmware to 0xe041000 - 0xe048524
  This initializes DDR RAM

- Image id 1 (FW_CONFIG_ID): to 0xe000000 - 0xe000326
  This is the DTB for something, probably for BL2? It seems to contain RISAF section definitions

- Image id 3 (BL31_IMAGE_ID) to 0xe000000 - 0x0e0124BD  (max size is 0x17000)
  This is the secure monitor TF-A

- Image id 19: (SOC_FW_CONFIG_ID): to 0x81fc0000 - 0x81fc39ff
  This is the DTB for BL31

- Image id 4: to 0x82000000 - 0x8200001c
  This is tee-header_v2.bin contains: magic=0x4554504f version=0x2 arch=0x1 flags=0x0 nb_images=0x1, and the address to load 0x82000000

- Image id 8: to 0x82000000 - 0x820d4398
  This is probably tee-pager_v2.bin 
  Note that tee-pageable_v2.bin is 0 bytes

- Image id 2: to 0x84400000 - 0x84417290
  This is the DTB for U-Boot (BL33)

- Image id 5: to 0x84000000 - 0x841560b0
  This is U-Boot (BL33)

