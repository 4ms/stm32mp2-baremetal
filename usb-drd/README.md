## USB DRD (Dual-Role) Example

This example uses the STM32MP257's USB3DR port as a **dual-role** port: it can
act as either a USB **device** or a USB **host**, and you switch between the two
at runtime by pressing the **User2** button.

- **Device mode** implements a CDC-ACM serial port that echoes back whatever you
  type (this is what the example does at startup).
- **Host mode** brings up an xHCI host controller, and enumerates whatever you plug
  in. If it's a USB-MIDI device it prints the incoming MIDI events to the
  console.

The STM32MP257 has two USB ports: the USB3DR port and the USBH port. This
example only uses the USB3DR port. That port is USB 3.0 capable (SuperSpeed plus
some USB-C PD capability), but here we drive it in USB 2.0 mode.

The device-side driver is a port of U-Boot's DWC3 driver (itself ported from
Linux), and the host side is a port of U-Boot's xHCI driver. We've kept them as
close to the U-Boot originals as possible (directory structure, file names,
etc.). In some places we had to fill in code that Linux had but U-Boot lacked.

### How the mode switching works

`main.cc` runs a loop that is in either Device or Host mode at any given time. It starts in
device mode. Pressing User2 requests a switch to the other mode, toggling between Device and Host modes.

Changing modes is only allowed when nothing is attached to the port. If you press
User2 while something is connected, the switch is refused and you'll see:

```
Cannot switch mode: host attached      (pressed in device mode, a host has VBUS up)
Cannot switch mode: device attached    (pressed in host mode, a device is enumerated)
```

Unplug the cable first, then press User2. The button is edge-triggered and
debounced (~300 ms), so a single press yields a single toggle.

Switching modes tears down the current role cleanly (stops the gadget/xHCI,
resets the DWC3 core and its DMA memory, and updates the Type-C CC/VBUS state)
before bringing up the other role. If a mode fails to initialize, the example
falls back to the other mode rather than hanging.

Each mode also logs state changes as they happen — Type-C CC/VBUS changes, the
USB link speed (Full/High/Low), and host configuration/de-configuration — so you
can watch what's going on over the console.

## Testing device mode (the default)

Connect a USB cable from the USB3DR port on the EV1 (CN12 — located near the
GPIO headers) to your computer. A CDC-ACM serial device appears; attach a
terminal to it:

```
# macOS
ls -l /dev/cu*
screen /dev/cu.usbmodem0000000000011 115200

# Linux
ls -l /dev/ttyACM0
screen /dev/ttyACM0 115200
```

Type characters and you should see them echoed back. That's device mode working.

## Testing host mode

Unplug the device cable, then press **User2** to switch to host mode. The
console prints:

```
--- Entering HOST mode (press User2 to switch) ---
USB Host ready. Plug in a device.
```

Now plug a USB device into the USB3DR port. The host enumerates it; if it's a
USB-MIDI device, the example starts printing MIDI events as you play:

```
Listening for MIDI...

MIDI [0] Note On   ch1   3c 64  (CIN=9)
MIDI [0] Note Off  ch1   3c 00  (CIN=8)
```

Hubs are skipped (their downstream devices enumerate on their own); a non-MIDI
device is enumerated but reported as "Not a MIDI device". Unplug the device and
the host goes back to waiting; press User2 (with nothing attached) to return to
device mode.

For host mode the port must source 5V on VBUS. On the EV1 the TCPP03-M20 gates
VBUS (see below); the custom devboard needs its adaptor board attached to supply
VBUS.

## Board selection

The Type-C port hardware differs per board, so the CC-termination/VBUS code is
selected at build time (see `typec.hh` for the interface):

```
make BOARD=ev1        # default: EV1 kit, TCPP03-M20 on I2C1 gates VBUS
make BOARD=devboard   # custom board v0.1: CC pins wired directly to UCPD1
```

Run `make clean` when switching boards (both configs share the build dir).

On the **EV1**, device mode relies on the TCPP03-M20's dead-battery Rd, and host
mode drives VBUS by enabling the TCPP03-M20 over I2C1. 
There is no VBUS sense on this board, so device mode treats the forced "VBUS
valid" bit as a host being present.

On the **devboard**, UCPD1 presents Rd (device mode) or Rp (host mode) on the CC
pins directly — no external port controller and no USB-PD messaging. The board
cannot source VBUS by itself; host mode needs the breakout board attached, whose
VBUS switch is driven by PB4 (either jack can then be used, since D+/D- and VBUS
are shared between them). VBUS presence is sensed on PH3 through a 430k/620k
divider. The breakout board's FUSB302B (I2C4 on PD10/PD11, INT on PD13) is left
unconfigured: its power-on default presents Rd on the second jack's CC pins,
which is harmless and even makes device mode work on that jack too.

Device mode defaults to High speed on the EV1 and Full speed on the devboard
v0.1 because the breakout board's D+/D- stubs make High speed fail. If you want
to use High speed with the devboard, remove the breakout board and re-compile with
`make USB_DEVICE_SPEED=high`.
