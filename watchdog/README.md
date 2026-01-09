## Watchdog SMC calling demonstration

This example demonstrates using SMC to reset the watchdog timer that OP-TEE
establishes (in the stock ST bootloader chain).

The watchdog needs to be "pet" every 32 seconds or less, or the system 
will reboot. This makes debugging difficult at times.

To pet the watchdog, an SMC must be called, which gets handled by OP-TEE.

Output:

```
Watchdog example
Petting the watchdog at 0 sec.
Petting the watchdog at 3 sec.
Petting the watchdog at 6 sec.
Petting the watchdog at 9 sec.
Petting the watchdog at 12 sec.
Petting the watchdog at 15 sec.
Petting the watchdog at 18 sec.
Petting the watchdog at 21 sec.
...
```
