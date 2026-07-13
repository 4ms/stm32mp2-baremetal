## Debug Load

The purpose of this project is to let you load an app via your debugger (JTAG/SWD).

All this does is clean/invalidate the caches, print "Ready" and then spin.

Compile and and install this on your SD card as normal. When you power up, you should see:

```
Ready
```

At this point you can use your JTAG/SWD debugger to load the main.elf or main.bin file for the project you're tyring to debug.

