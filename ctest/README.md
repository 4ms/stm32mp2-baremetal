## Minimal Build project

All this project does is print an 'A' to the UART, and then hang forever. 

Why do we want this? A simple 'hello world' project like this is useful for debugging and as a starting place to verify u-boot is loading your project. 

To use, first make sure u-boot is installed on an SD-card, as usual (see README in the project root directory). 

Then, in this directory run:

```
make
```

In the build/ dir, you should see the main.uimg file. Copy it to the 11th partition of the SD-card, as usual (again, see README in the project root dir).

Reboot your board with a UART-to-USB cable connected, and watch u-boot's startup messages scroll by in a terminal. Then... wait for it...

```
A
```

ITS ALIVE!!!!

