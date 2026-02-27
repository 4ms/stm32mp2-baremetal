## Minimal Build project

All this project does is print three characters to the UART, and then hang forever. 

Why do we want this? A simple 'hello world' project like this is useful for
debugging and as a starting place to verify the bootloader is loading your project. 

Build and load as described in the main README.

Reboot your board with a UART-to-USB cable connected, and watch TF-A's
startup messages scroll by in a terminal. Then... wait for it...

```
MP2
```

ITS ALIVE!!!!

