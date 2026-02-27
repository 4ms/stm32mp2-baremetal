# stm32mp2-baremetal

This contains works-in-progress exploring using the STM32MP2xx chips in a
bare-metal context on all cores.

All projects are designed for the STM32MP257F-EV1 board, but changing to your
own board should be trivial.

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

This repo depends on our modified TF-A bootloader, so you need to clone that repo as well.
Keep these organized with a parent dir.

```bash
# Create the project parent dir
mkdir mp2-dev
cd mp2-dev

# Clone this repo
git clone https://github.com/4ms/stm32mp2-baremetal

# Clone the TF-A bootloader:
git clone https://github.com/4ms/tf-a-stm32mp25
```

Now you should have a directory structure like this:

```
mp2-dev/
   |_ stm32mp2-baremetal/
   |_ tf-a-stm32mp25/
```

## Prerequisites

To build you need the aarch64-none-elf-gcc toolchain. Versions 12.3, 13.1, and 14.2 have been tested but
probably any later version will also work (please open an issue if you find a version that doesn't).

This should be on your PATH, so you can run them like this:

```bash
aarch64-none-elf-gcc --version
```


# Exception Level 3 (EL3) and Secure state

All examples run in EL3 secure state. For this to happen, the bootloader needs
to load the app in EL3 secure. You must use a our fork of TF-A BL2 (FSBL) as
the sole bootloader. This FSBL is greatly simplified (much easier to build)
from the stock TF-A + OP-TEE + U-Boot bootloader chain. 

Our TF-A fork initiliazes DDR RAM and then loads your application as a secure
payload. There is no OP-TEE, U-Boot, or BL31 Secure Monitor running
anymore. You only need the TF-A BL2 FSBL and your application.

Obviously, running an application this way is inherently less secure. If you
plan to use this in a production device, you should carefully consider the
security requirements before running your main application in EL3 Secure state.
For hobbyist projects, or some embedded projects with no connectivity (e.g. an
audio FX unit without USB/Wi-Fi/etc), or for designing your own Secure Monitor,
running in EL3 Secure is more convenient.


# Prepare your SD Card

For simplicity we will boot from the SD card for all these examples.

To prepare an SD card, use the partition-sdcard.sh script:

```bash
partition-sdcard.sh /dev/diskX
```

This will wipe the card clean, so double-check you use the right device path!

The script sets up 9 partitions on the card. These aren't all needed, but we 
do this for compatibility with STM's stock SD card. 
Blocks are 512B, and the card must have a GPT partition scheme.

The important partitions are:

- Partition 1: starting block 34, size 256kB, name "fsbla1", GUID 8DA63339-0007-60C0-C436-083AC8230908
   - This is where the FSBL is loaded (TF-A BL2). The starting block must be 34 (address 0x4400)
- Partition 2: starting block 546, size 256kB, name "fsbla2", same GUID as above
   - This is a redundant copy of partition 1. Starting block must be 546 (address 0x44400)
- Partition 5: name "fip-a", GUID 19D5DF83-11B0-457B-BE2C-7559C13142A5
   - This is where the FIP file lives, which contains the DDR init firmware and your app.

The partition types are critical for the above three partitions. The BOOTROM will reject
the card if the GUID is not set correctly. Also, TF-A will reject the FIP file if its 
partition does not have the expected GUID.

I believe the partition names are important too, but I haven't verified this.


# Build and install the bootloader


You need to install TF-A BL2 on partitions 1 and 2 of your SD Card.
Then, you need to create a FIP file from your app binary and the DDR firmware
binary and install that onto partition 5.

See the TF-A README for details.

## Build FSBL (BL2):

Build `tf-a-stm32mp257f-ev1.stm32`, which is the FSBL (first stage bootloader).
Then install it onto your SD card. Change the `diskX1` and `diskX2` below to
the device path to your SD card partitions 1 and 2:

```bash
cd tf-a-stm32mp25
# [Configure your TF-A build for USART2 or USART6 here]
./build.sh
sudo dd if=build/stm32mp2/release/tf-a-stm32mp257f-ev1.stm32 of=/dev/diskX1
sudo dd if=build/stm32mp2/release/tf-a-stm32mp257f-ev1.stm32 of=/dev/diskX2
```

While you're in the tf-a project, build the fiptool:
```bash
make PLAT=stm32mp2 BAREMETAL_IMAGE_LOADER=1 fiptool
```

On some macOS systems, you will need to do this:

```bash
./buildfiptool.sh
```

You will need the fiptool later after building your application.

# Build and install the application

## Configuring a project

The only thing to configure currently is the choice of UART.

### Console via ST-LINK

By default all console logging (print/printf) goes to USART2, which is
connected to the ST-LINK adaptor via the USB-C jack.
To use this, make sure in your Makefile it says this (or doesn't set anything):

```
UART_CHOICE := 2
```

### Console via GPIO Expander header

The other option is to use USART6 via the GPIO Expander port. This is the best
way to have a UART console if you are using an external debugger
(TRACE32/J-Link) with the MIPI-10 header. 

You will need to attach a USB-UART dongle to the GPIO Expander:
- Pin 6: GND
- Pin 8: TX (mp2->computer)
- Pin 10: RX (mp2<-computer)


To use these pins, put this in your Makefile:
```
UART_CHOICE := 6
```

or you can specify it at build time:

```bash
make UART_CHOICE=6
```

## Building a project

Build a project by running `make` in its directory.

```bash
cd stm32mp2-baremetal
cd minimal_boot
make
```


Ensure the project was built:
```bash
ls -l build/main.bin
ls -l build/main.elf
```


## Installing the application on the SD card

The easiest way to run your project is to copy the main.bin file onto the SD
card, as part of the FIP file. See the TF-A README for details.

In short, you do this (but change the --bm-fw path to be the project you want to load,
and also change `diskX5` to partition 5 of your SD card device)

```bash
cd ../tf-a-stm32mp25
tools/fiptool/fiptool --verbose create \
	--ddr-fw drivers/st/ddr/phy/firmware/bin/stm32mp2/ddr4_pmu_train.bin \
	--bm-fw ../stm32mp2-baremetal/minimal_boot/build/main.bin \
	build/stm32mp2/release/fip.bin

sudo dd if=build/stm32mp2/release/fip.bin of=/dev/diskX5
```


## Running the program

Install the SD card into the EV1 board, and power on the board.

Connect the USB jack to your computer (or if you're not using ST-LINK, then
connect your USB-UART dongle to the GPIO header and to your computer).

Run a terminal console program on your computer, and connect to the TTY port at
115200 8N1.

For example, using minicom on macOS (the device number varies): 

```bash
minicom -D /dev/cu.usbmodem1102
```

Now, press Reset on the EV1. You should see messages from TF-A and then your app!

If you don't see anything, verify you built TF-A and you app with the right UART 
selected (USART2 for ST-LINK, USART6 for GPIO Expander header + dongle).


# Debugging

While it's easy to copy your binary onto the SD card and then reboot,
it's not a good workflow if you're making lots of changes. A faster workflow
is to load over SWD with a debugger. This has the added benefit of letting 
you use a debugger to debug your program.

There are two main ways of doing this: 
- using the EV1 board's built-in ST-LINK with just a USB cable
- or using the EV1 board's 10-pin SWD header (CN22 MIPI-10) with an external
  debugger such as the J-Link or TRACE32.
 

First, you will need to load the minimal_boot project onto the SD card. Make sure
it boots up and says "MP2" and then hangs.

While it's hanging, you can load or reload any of the other projects over SWD.
You can just leave the SD card installed in the EV1, and reboot with the Reset button.
Loading a new project just means waiting until it boots (1-2 seconds)
and then loading the binary or elf file with your debugging software.

## Loading an app via SWD via the USB jack (ST-LINK)

The Discovery and Eval boards have an SWD connection via the USB jack marked
"ST-LINK". This USB connection provides both a ST-LINK debugger interface, as
well as a console UART.

1. Connect your computer to this USB jack. Power on with the SD card installed,
   and open a console terminal on your computer as described above in "Running the program".

2. Start openocd. The scripts for the stm32mp2 chips are included in this repo,
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

3. Connect via gdb in another terminal window:

```bash
cd [example project dir]
make
aarch64-none-elf-gdb build/main.elf
```

If gdb complains that it has a security setting preventing it from running the
.gdbinit script, then either follow the instructions it provides to allow this
directory, or run the command contained in the .gdbinit file yourself:

```
target extended-remote localhost:3333
load
```

You can then do normal gdb stuff like this:

```
thbreak main
run
disassemble
list main
si
n
x/64i 0x88000040
continue
```

### Resetting

If you re-compile and need to load the new binary, unfortunately the best way
I've found is to press the hard reset button on the EV1 board. Usually openocd
will re-connect, but if it doesn't then you have to unplug/plug the USB cable
so it can re-enumerate the ST-LINK USB device. After doing this, you need to
quit gdb and re-start it. This process is could use improvement (especially
losing the gdb history), so if anyone has a better way, please let me know
(open an issue, PR, or comment). I've tried various commands like `monitor
halt` and `monitor reset` but they inevitably make it harder to connect.


## Debugging via SWD with the MIPI-10 header (J-Link or TRACE32)

If you prefer to use an external debugger like the J-Link or the TRACE32, you can 
connect via the MIPI-10 header CN22.
The header has the NRST pin, but no JTRST pin, so JTAG is not reliable (YMMV).
I found SWD to be a better connection because of this.
I've had excellent results with the TRACE32 debugger, and mixed results with
the J-Link.

### UART connection

This header is only usable if you install jumper JP3, which 
puts the ST-LINK controller IC into reset. Therefore you cannot use USART2
while using the MIPI-10 JTAG/SWD header, so USART6 is the best way to get
a UART console.

You will need a USB-to-UART dongle. See the above section on UART selection for
the pins to connect.


### Powering the board

You also need to power the board with the barrel plug, not the USB jack. To do
this, move the jumper on JP4 to the top position and plug in a 5V/3A DC power
supply (I found that a 12V/1.5A power supply worked fine, too, and confirmed
with the datasheet of the DCDC converter that this voltage is OK).

### Debugging software

#### J-Link

With a J-Link, you can use JLink's GDB server, and then connect via gdb. Follow
the instructions on SEGGER's wiki to set that up (it's fairly easy to do if you
use their GDB Server GUI app). Or, you can use Ozone, which is also easy to use.
I ran into issue when resetting my device, but I didn't try too hard to figure out
the problem. I was able to launch the gdb server and connect via gdb. Sometimes 
I had to do `load` twice in a row for some reason. Also, if I immediately ran 
the program after loading, it would work, but if I stepped through the startup
code, it would crash. It may be that something is not aware of MMU settings
or something like this, as it seems to work fine once the MMU is set up.

#### TRACE32

The TRACE32 debugger will connect to the EV1 board reliably. I've included a
t32 cmm file in scripts to help.



# Misc Info

## Support files:

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

