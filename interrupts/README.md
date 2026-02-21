## Interrupt demonstration

This project defines five IRQ handlers, with different priorities and
demonstrates that nested interrupts works.

Compile and load the program. When prompted, press a key into the console to
trigger the UART IRQ. Or, press button USER2 on the EV1 board.

Pressing a keyboard key will trigger the USART interrupt handler, and pressing
the USER2 button will fire an SGI4 interrupt. Both these interrupts have the
same low priority (3.1). Both these handlers do the exact same thing, which is
to trigger an SGI2 interrupt. The SGI2 handler has a lower priority (2.3) and
should run immediately, interrupting the current handler. 

The SGI2 handler triggers an SGI1 interrupt, which has an even lower priority
(0.0) and should run immediately, interrupting the SGI2 handler.

After the SGI1 handler exits, it returns to the SGI2 handler. Then the SGI2
handler exits and reutrns to either the UART handler or the SGI4 handler.

This handler then triggers an SGI3 interrupt, which has the same priority as
the UART IRQ and SGI4 handlers but with a lower sub-priority (3.0). Recall from
your GIC knowledge that a pending interrupt will not interrupt an active
interrupt with the same priority, regardless of sub-priority. So the pending
interrupt (SGI3) waits to run until after the current active interrupt (SGI4 or
UART IRQ) exits.

Make sure to set the UART_CHOICE build flag in the Makefile:

For GPIO Expander pins 6,8,10:
```
UART_CHOICE := 6
```

For ST-LINK:
```
UART_CHOICE := 2
```


Expected output: (notes in `[brackets]` are actions taken by you)

```
Press a key to trigger USART2 RX IRQ
Or press button USER2

[user types 'a']

> USARTx handler started
Received:
   > a
Now sending SGI2: Expect SGI now:
> SGI2 handler started
  SGI2: Sending SGI1
 > SGI1 handler started
 < SGI1 handler ended
< SGI2 handler ended
Now sending SGI3... (should happen after USARTx exits)
< USARTx handler ended: Expect SGI3 now:
> SGI3 handler started
< SGI3 handler ended

[user presses button USER2 on the EV1 board]

> User2 button pressed -> SGI4 started
Sending SGI2: Expect SGI2 now:
> SGI2 handler started
  SGI2: Sending SGI1
 > SGI1 handler started
 < SGI1 handler ended
< SGI2 handler ended
Sending SGI3... (should happen after SGI4 exits)
< SGI4 handler ended: Expect SGI3 now:
> SGI3 handler started
< SGI3 handler ended
```


### Further work

We could test if FPU registers are properly saved.

We could also test disabling interrupts, then firing two interrupts with the
same priority but different sub-priorities. Then unmasking interrupts and
verifying the one with the lower sub-priority value runs first.

We could verify all combinations of priority and sub-priority, ensuring the
breakpoint register behaves as expected.
