## Interrupt demonstration

This project defines four IRQ handlers, with different priorities and demonstrates that nested interrupts works.

Compile and load the program. When prompted, press a key into the console to trigger the UART IRQ.

The UART IRQ handler will trigger an SGI2 interrupt, which has a lower priority and should run immediately.

The SGI2 interrupt triggers an SGI1 interrupt, which has an even lower priority and should run immediately.

Finally, the UART IRQ handler triggers an SGI3 interrupt, which has the same priority as the UART IRQ but 
with a lower sub-priority. The SGI3 handler should run after the UART handler exits.


```
Press a key to trigger USART2 RX IRQ
> USART2 handler started
Received:
   > a
Now sending SGI2: Expect SGI now:
> SGI2 handler started
Sending SGI1
> SGI1 handler started
< SGI1 handler ended
< SGI2 handler ended
Now sending SGI3... (should happen after USART2 exits)
< USART2 handler ended: Expect SGI3 now:
> SGI3 handler started
< SGI3 handler ended
```
