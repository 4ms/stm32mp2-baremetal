# stm32mp2-baremetal

This contains baremetal example projects for the Cortex A35 cores of the
STM32MP257.

The STM32MP2xx (aka "MP2") chips are powerful 64-bit 1.5GHz dual-core
processors with access to fast DDR4 RAM, GPU, NEON instructions and a host of
peripherals that are (mostly) compatible with the peripherals from other
high-end STM32 MCUs such as the MP1, H7 and F7 series.

While traditionally used to run Linux or other OS's, these projects show that
it's possible to run your own programs on the MP2 chips without any operating
system. Whether or not a baremetal approach is a good idea for your project is
a decision you should weigh out carefully, but the purpose of this repo is 1)
explore and document the low-level workings of this architecture and the
peripherals, and 2) help you get started making a project on these chips.

This project is modeled after my [stm32mp1-baremetal](https://github.com/4ms/stm32mp1-baremetal) project.

Most projects are designed for the STM32MP257F-EV1 board, and some projects
have an option to be run on a custom board I use (see the README for each project).

Currently working examples:

  - EL3 Secure state
  - Nested interrupts (using the GIC)
  - MMU configuration
  - DMA memory-to-memory transfers
  - Clock configuration to run at full speed (1.5GHz for CA35, 400MHz for CM33)
  - SAI (audio)
     - Using DMA
     - SAI TX (DAC audio out)
     - SAI RX (ADC audio in)
  - I2C
  - SMP Multi-core (dual CA35):
     - Startup code for each core
     - Core-specific interrupts (SGI)
  - Loading and running firmware on the M33 core
  - USB dual-role host/device
     - Device via USB3DR
     - Host via USB3DR
     - USB Hub support
  - ADC running with DMA
  - LTDC + LVDS interface 
  - LTDC + MIPI DSI interface
  - LTDC + RGB parallel interface
  - 3D and 2D GPU rendering pipeline
  - HSEM hardware semaphore

![](docs/gpu-ltdc-demo-crop.gif)


TODO:
  - IPCC (A35<=>A35 and A35<=>M33)
  - Loading and running firmware on the M0+ core 
  - SDMMC read/write
  - XSPI read/write (requires custom board with a flash chip)

# Quick Start

#### Step 0: Prerequisites

To build you need the aarch64-none-elf-gcc toolchain. Versions 12.3, 13.1,
14.2, and 15.3 have been tested but probably any later version will also work
(please open an issue if you find a version that doesn't).

It should be on your PATH, so you can run it like this:

```bash
aarch64-none-elf-gcc --version
```

You also need to clone this repo:

```bash
git clone https://github.com/4ms/stm32mp2-baremetal
```

And you will need an SD card.


#### Step 1: Prepare an SD Card 

To prepare an SD card, use the partition-sdcard.sh script:

```bash
scripts/partition-sdcard.sh /dev/diskX
```

This will wipe the card clean, so double-check you use the right device path!

#### Step 2: Install the bootloaders on the SD card

The bootloader is TF-A, which can be built here: [TF-A bootloader](https://github.com/4ms/tf-a-stm32mp25).
For convenience, there are pre-built releases for the EV1 board [here](https://github.com/4ms/tf-a-stm32mp25/releases)

Download (or build) the `tf-a-stm32mp257f-ev1.stm32` and `fip.bin` files.
Then install them on the SD card like this:

```bash
sudo dd if=/path/to/tf-a-stm32mp257f-ev1.stm32 of=/dev/diskX1
sudo dd if=/path/to/tf-a-stm32mp257f-ev1.stm32 of=/dev/diskX2
sudo dd if=/path/to/fip.bin of=/dev/diskX5
```

In the commands above, specify the path to the files and the disk device name.

#### Step 3: Build and install an example project

Build any project by running `make` in its directory.

```bash
cd stm32mp2-baremetal
cd minimal_boot
make
```

This will create a `main.uimg` file in the `build/` dir. 

Copy the `main.uimg` file to the "app" partition. From the project
directory, either:

```bash
make flash SD=/dev/diskX      # finds the "app" partition and dd's to it
```

or do it by hand (the app partition is partition 9 if the card was set up
with partition-sdcard.sh):

```bash
sudo dd if=build/main.uimg of=/dev/diskX9
```

#### Step 4: Boot the board

Insert the SD card into the EV1's card slot, set the BOOT switches to SD boot (down down down up -- see the EV1 docs),
and power it up. Attach a USB-C cable to your computer and open a console to the tty device that
shows up when you attach the cable. I use `minicom`, but screen, picocom and other options exist.

```bash
minicom -D /dev/cu.usbmodem1105
```

Press the reset button and you should see something like this:
```
NOTICE:  CPU: STM32MP257FAI Rev.?
NOTICE:  Model: STMicroelectronics STM32MP257F-EV1 Evaluation Board
NOTICE:  Board: MB1936 Var1.0 Rev.C-01
INFO:    Reset reason: Power-on reset (por_rstn) (0x2035)
INFO:    PMIC2 version = 0x11
INFO:    PMIC2 product ID = 0x20
INFO:    FCONF: Reading TB_FW firmware configuration file from: 0xe011000
INFO:    FCONF: Reading firmware configuration information for: stm32mp_fuse
INFO:    FCONF: Reading firmware configuration information for: stm32mp_io
INFO:    Using SDMMC
INFO:      Instance 1
INFO:    Boot used partition fsbl1
NOTICE:  BL2: v2.10-stm32mp2-r2.0(release):baremetal-v0.1(ba0a1568)
NOTICE:  BL2: Built : 23:44:40, Jul  9 2026
INFO:    BL2: Loading image id 26
INFO:    Loading image id=26 at address 0xe041000
INFO:    Image id=26 loaded: 0xe041000 - 0xe048524
INFO:    BL2: Doing platform setup
INFO:    RAM: DDR4 2x16Gbits 2x16bits 1200MHz
INFO:    Memory size = 0x100000000 (4096 MB)
INFO:    BL2: Skip loading image id 27
INFO:    Loaded app 'stm32mp2-baremetal image': 64 bytes at 88000000, entry 88000000
NOTICE:  BL2: Booting BL31
INFO:    Entry point address = 0x88000000
INFO:    SPSR = 0x3cd
MP2
```

The MP2 at the end is the confirmation the `minimal_boot` example program ran.

Each project has a README that describes what should happen. Some require
interactivity (like `interrupts`), some require you to connect external devices
(like `usb-drd` and `hal-audio`), and some just run on their own. 


# Detailed description


## TF-A bootloader

This repo depends on our [modified TF-A
bootloader](https://github.com/4ms/tf-a-stm32mp25), and as you start to extend
these example projects to meet your own needs, you may need to change and
re-build TF-A as well. 

See the [TF-A README](https://github.com/4ms/tf-a-stm32mp25/blob/v2.10-stm32mp2-baremetal/readme.md)
for instructions on how to build and customize that project.

When TF-A boots, it initializes the PMIC (power controller IC), turns on 
various necessary peripherals, and sets up the security settings. Then it
loads and executes a DDR RAM training binary. 
Finally, it reads from the 'app' partition of the SD card and loads your
application. It reads the uimg header (load address, entry point,
size, CRCs) and starts the app. 

## Console

The UART communicates with your computer, which is running a console program. This lets 
you send/receive data from the STM32 chip, and is the primary method for verifying
these examples.

### Console via ST-LINK

If you're using an EV1 board, then the UART is accessed over USB, using a normal
USB cable going between the EV1's ST-LINK USB jack (CN21) and your computer.
This is UART 2, and is the default if you don't specify any `UART=` option when
building from the command-line, and don't specify any `UART := ` override in
the Makefile.

### Console via GPIO Expander header

Another alternative with the EV1 is to use a USB-UART dongle, with the
TX/RX/GND pins of the dongle attached to the GPIO Expander header of the EV1.
The other end of the dongle connects to your computer's USB port. These
UART-USB dongles are cheap and easy to buy on many component stores or even
Amazon. 

This is the best way to have a UART console if you are using an external
debugger (TRACE32/J-Link) with the MIPI-10 header (see Debugging section
below).

You will need to attach a USB-UART dongle to the GPIO Expander:
- Pin 6: GND
- Pin 8: TX (mp2->computer) — PF13
- Pin 10: RX (mp2<-computer) — PF14

Building with the USART6 chosen is done from the command line like this:

```bash
make UART=6
```

Or you can specify it in the Makefile, by adding this line (or replacing an existing one):

```make
UART := 6
```

### Console on a custom board

Finally, if you have a custom board, you can use a USB-UART dongle attached to
any UART your board exposes. Add your UART choice in `shared/uart_print.c`.
Follow the example of how I added USART1 for my custom board. Building for a
custom board works just like it does for the GPIO Expander UART: just build
with `make UART=#` or add `UART := #` to the command-line. For example, my
custom devboard uses USART1, so I build with `make UART=1 `

### Console program

You need to run a terminal console program on your computer, and connect to the TTY port at
115200 8N1.

For example, using minicom on macOS (the device number varies): 

```bash
minicom -D /dev/cu.usbmodem1102
```

When you boot up the board, you should see messages from TF-A and then from
your app.

If you don't see anything, verify your app was built with the right `UART`
(`UART=2` for ST-LINK, `UART=6` for the GPIO Expander header + dongle).

Note that TF-A prints its own early boot messages to *its* console, which is
independent of the UART your app selects. Only the app's `print()` output is
changed by the `UART=` build option. So if you are not using the ST-LINK UART2,
then you will need to re-build TFA in order to see its console messages. It's
also safe to ignore TF-A boot messages, but if you run into problems booting
you probably will want to see them to debug.


## BOARD selection

By default all projects run on the EV1, but some examples (usb-drd and adc)
have alternative configurations that you can select by setting the BOARD
argument. Read the README for the project. Do not blindly set BOARD to a
board you aren't using. Setting a custom board could **damage your EV1** (for
example, one project enables an internal voltage reference on a pin that the
EV1 supplies voltage to).


# Debugging

While it's easy to copy your binary onto the SD card and then reboot,
it's not a good workflow if you're making lots of changes. A faster workflow
is to load over SWD or JTAG with a debugger. This has the added benefit of
letting you use a debugger to debug your program.

First, you will need to load the `debug_load` project onto the SD card. Make sure
it boots up and says "Ready" and then hangs.

While it's hanging, you can load or reload any of the other projects over SWD.
You can just leave the SD card installed in the EV1, and reboot with the Reset button.
Loading a new project just means waiting until it boots (<1 second usually)
and then loading the binary or elf file with your debugging software.

There are three ways to connect a debugger to the EV1 board:
1. Connecting a USB cable to the EV1's ST-LINK via the USB jack
2. Connecting an ST-LINK-V3 debugger to the EV1's SWD header (CN22 MIPI-10).
3. Connecting a J-Link or TRACE32 external debugger to the EV1's SWD header (CN22 MIPI-10).

The first way (direct USB cable to the EV1) connects to an on-board ST-LINK
circuit on the EV1. The second way (using an external ST-LINK-V3 debugger) is
essentially the same thing, so follow the steps in the next section for method
1 or 2. The third way has a different setup, so skip down to *Debugging via the
MIPI-10 header (J-Link or TRACE32)*.

## Debugging with the EV1's ST-LINK via the USB jack (or with an external ST-LINK-V3)

1. Connect your computer to the ST-LINK USB jack on the EV1. If you're using an
   external ST-LINK-V3 with a custom board, then connect it to the 10-pin SWD
   MIPI-10 header (CN22). To use the MIPI-10 header, you may need to install
   jumper J13, which resets the MCU that handles the internal ST-LINK (however
   some users have reported not needing to do this). If that's the case, then
   follow the *Debugging via the MIPI-10 header* section about power and UART.

2. Power on with the SD card installed, and open a console terminal on your
   computer as described above. 

3. Start openocd. The scripts for the stm32mp2 chips are included in this repo,
   as well as the openocd config file (`openocd.cfg`). If you laucnh openocd in a
   new terminal window from the repo root, it will find the scripts automatically:

```bash
cd stm32mp2-baremetal     # be in this dir for openocd to find the scripts automatically
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
directory, or run the commands contained in the .gdbinit file yourself:

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

If you re-compile and need to load the new binary, gdb and openocd get confused
with the multiple cores and caches, and with memory access via AXI bus vs.
through a processor core, and it usually doesn't work to do a simple `monitor
reset` like can be done on more simple MCUs. Caches need to be invalidated and
disabled and cores need to be parked, which is more easily done in assembly
than with openocd commands. 

There's a helper script in `scripts/stlink/` which you can invoke with:

```bash
make flash-stlink
```

This will reset the device and flash the current project. If you have an openocd
server running, it will use that, so as to not disconnect your gdb session. Otherwise
it will launch openocd, run the script, and kill the openocd process afterwards.

## Debugging via the MIPI-10 header (J-Link or TRACE32)

If you prefer to use an external debugger like the J-Link or the TRACE32, you
can connect to the EV1 via the MIPI-10 header CN22.

The header has the NRST pin, but no JTRST pin, so JTAG is not reliable. I found
SWD to be a better connection because of this. I've had excellent results with
the ST-LINK-V3 and TRACE32 debuggers, and mixed results with the J-Link.

### UART connection

I found that the SWD header is only usable if you install jumper JP3, which
puts the ST-LINK controller IC into reset. Therefore I cannot use USART2 while
using the MIPI-10 JTAG/SWD header, so USART6 is the only way I am able to get a
UART console.

However, one user on the ST forums described that they are able to use the SWD header
without disabling the ST-LINK controller IC: https://community.st.com/stm32-mpus-products-and-hardware-related-39/connect-a-debugger-to-stm32mp257f-ev1-board-137825

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
I ran into an issue when resetting my device, but I didn't try too hard to figure out
the problem. I was able to launch the gdb server and connect via gdb. Sometimes 
I had to do `load` twice in a row for some reason. Also, if I immediately ran 
the program after loading, it would work, but if I stepped through the startup
code, it would crash. It may be that something is not aware of MMU settings
or something like this, as it seems to work fine once the MMU is set up.

#### TRACE32

The TRACE32 debugger will connect to the EV1 board reliably. I've included a
t32 cmm file in scripts to help in `scripts/t32-mp257.cmm`. 
You can also reset and flash with this helper:

```bash
make flash-t32
```

which requires TRACE32 to be running with the python rcl started (see TRACE32 docs),
and the TRACE32 python module to be installed. This script just runs a python script:

```bash
python3 scripts/flash_t32.py [elf file]
```


# Exception Level 3 (EL3) and Secure state

All examples run in EL3 secure state. For this to happen, the bootloader needs
to load the app in EL3 secure. You must use our fork of TF-A BL2 (FSBL) as
the sole bootloader. This FSBL -A initializes DDR RAM and then loads your
application as a secure payload. There is no OP-TEE, U-Boot, or BL31 Secure
Monitor running. You only need the TF-A BL2 FSBL and your application.

Obviously, running an application this way is inherently less secure. If you
plan to use this in a production device, you should carefully consider the
security requirements before running your main application in EL3 Secure state.
For hobbyist projects, or some embedded projects with no connectivity (e.g. an
audio FX unit without USB/Wi-Fi/etc), or for designing your own Secure Monitor,
running in EL3 Secure is more convenient. 

If you want to see examples running EL1 non-secure, then see the [el1-ns branch](https://github.com/4ms/stm32mp2-baremetal/tree/el1-ns).
This requires OP-TEE to be modified to allow for non-secure access to the peripherals
and memory regions that the examples use.

Most of these examples should be agnostic to the EL level, and where it matters
I've added `#ifdef RUN_EL3` to the code.


# Misc Info

## Support files:

The SVD files are here:
https://github.com/STMicroelectronics/meta-st-stm32mp/tree/scarthgap/recipes-devtools/cmsis-svd/cmsis-svd


OpenOCD stuff here:
https://github.com/STMicroelectronics/meta-st-stm32mp/tree/scarthgap/recipes-devtools/openocd

The CA35 CMSIS device headers are missing from the STM32MP2 Cube HAL (only the M33 and M0+ are present).
But they are here in the DDR Firmware repo:
https://github.com/STMicroelectronics/STM32DDRFW-UTIL/tree/main/Drivers/CMSIS/Device/ST/STM32MP2xx/Include
I've copied them into this repo in the shared/cmsis-device dir.

There is a serious issue in the stm32mp2xx.h file provided by ST, that breaks
several HAL peripherals. The issue is the POSITION_VAL function macro does not
work on 64-bit systems. This has been corrected in the version in this repo.

## SD Card info

From TF-A boot, the SD card is organized:
```
Partition table with 10 entries:
1: fsbla1                              4400-443fc
2: fsbla2                              44400-843fc
3: metadata1                           84400-c43fc
4: metadata2                           c4400-1043fc
5: fip-a                               104400-5043fc
6: fip-b                               504400-9043fc
7: u-boot-env                          904400-9843fc
8: bootfs                              984400-49843fc
9: app                                 4984600-59845fc
10: fatfs                              (rest of card)
```

The `scripts/partition-sdcard.sh` script sets up 10 partitions on the card. These aren't all needed, but we 
do this for compatibility with STM's stock SD card. 
Blocks are 512B, and the card must have a GPT partition scheme.

The important partitions are:

- Partition 1: starting block 34, size 256kB, name "fsbla1", GUID 8DA63339-0007-60C0-C436-083AC8230908
   - This is where the FSBL is loaded (TF-A BL2). The starting block must be 34 (address 0x4400)
- Partition 2: starting block 546, size 256kB, name "fsbla2", same GUID as above
   - This is a redundant copy of partition 1. Starting block must be 546 (address 0x44400)
- Partition 5: name "fip-a", GUID 19D5DF83-11B0-457B-BE2C-7559C13142A5
   - This is where the FIP file lives, which contains the DDR init firmware.
     It is written once and doesn't change when you rebuild your app.
- Partition 9: size 16MB, name "app"
   - This is where your application (`build/main.uimg`) lives. TF-A finds this
     partition by its GPT name, so the partition number doesn't matter.

The partition types are critical for partition 1, 2, and 5. The BOOTROM will reject
the card if the GUID is not set correctly. Also, TF-A will reject the FIP file if its 
partition does not have the expected GUID.


