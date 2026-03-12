## USB DRD Device Example

This example demonstrates using the USB3DR port as a USB device. There is a CDC ACM device implemented.

The STM32MP257 has two USB ports: the USB3DR port and the USBH port. 
In this example we only use the USB3DR port. This port is USB 3.0 capable
(including SuperSpeed and some USB-C PD capabilities) but also can operate in
USB 2.0 mode (which is what we use here).

The driver is a port of U-Boot's DWC3 driver, which itself was ported from Linux. We've kept
it as similar as possible to the U-Boot driver (including directory structure, file names, etc).
In some cases we had to implement bits of code that Linux had, but U-Boot lacked.

To test this, connect a USB cable from the USB3DR port on the EV1 (CN12 -- located near the GPIO headers), 
to your computer. Open a terminal window and attach a console to the newly created device:

```
# MacOS
ls -l /dev/cu*
screen /dev/cu.usbmodem0000000000011 115200

# Linux
ls -l /dev/ttyACM0
screen /dev/ttyACM0 115200
```

Type characters and you should see them echo'ed back to you. That's it, it's working!

