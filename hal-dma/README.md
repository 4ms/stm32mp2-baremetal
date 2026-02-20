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


Make sure you set the UART_CHOICE in the Makefile to match your choice of UART:
For ST-LINK:
```
UART_CHOICE := 2
```

For GPIO Expander pins 6=GND/8=TX
```
UART_CHOICE := 6
```


The example should work if you change the memory region to be in DDR, SYSRAM,
SRAM1, etc. You can do that by changing the `__attribute__((section())))` name at
the top of main.cc. The linkscript.ld has the section names. For example:

For SYSRAM (0x0E000000 alias):
```c++
alignas(64) __attribute__((section(".b_ns_sysram"))) std::array<uint32_t, BufferWords> src_buffer;
alignas(64) __attribute__((section(".b_ns_sysram"))) std::array<uint32_t, BufferWords> dst_buffer;
```

For RETRAM (0x30080000 alias):
```c++
alignas(64) __attribute__((section(".s_retram"))) std::array<uint32_t, BufferWords> src_buffer;
alignas(64) __attribute__((section(".s_retram"))) std::array<uint32_t, BufferWords> dst_buffer;
```

Or for DDR RAM (in the 0x90000000 region):
```c++
alignas(64) __attribute__((section(".data"))) std::array<uint32_t, BufferWords> src_buffer;
alignas(64) __attribute__((section(".data"))) std::array<uint32_t, BufferWords> dst_buffer;
```

