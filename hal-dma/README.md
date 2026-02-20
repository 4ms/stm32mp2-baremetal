## HAL

This example demonstrates using the HAL DMA driver to copy memory contents.

The memory region copied is cacheable, so it also demonstrates cache maintenance operations.

Finally, in order for ST's HAL to work, there needs to be a SysTick established, so this
example also does that.

When running, you should see this:

```
HAL Example
Tick = 7212
Starting a DMA memory to memory transfer
Preparing dst buffer: 0x9000F400
Preparing src buffer: 0x9000F700
Got DMA IRQ at tick 7218
DMA transfer success! dst_buffer matches src_buffer
Tick = 7261
Tick = 7503
Tick = 7745
...
```

The tick values are in milliseconds, you can use a stopwatch to verify the values increment by 1000 every second.

The example should work if you change the memory region to be in DDR, SYSRAM,
SRAM1, etc. You can do that by changing the `__attribute__((section())))` name at
the top of main.cc. The linkscript.ld has the section names.
