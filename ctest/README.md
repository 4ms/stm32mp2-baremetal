## C Runtime Test

This project clears the bss region, and has the link script setup so the static/global data section 
is initialized. It also sets up the stack. All these things are required for C (or C++) functions to work.

It also sets up the MMU, though this is not being demonstrated.

The output should be:

```
MP2
EL3
>XYZ
```


The "MP2" is printed just for reference that the UART is working.

The EL level tells us if we're in EL1, EL2, or EL3. EL1 or 2 means we came from U-Boot. EL3 means
we're booting directly from TF-A as the secure payload. If we start in EL2, then the startup script
drops us down to EL1, so you'll see "EL2 >EL1"

The "X" is a read-only global, which demonstrates read-only .data section is correct.

The "Y" is a read/write global, which demonstrates the .data section is correct.

The "Z" is a written if the zero-initialized global is actually 0. If it is non-zero,
then the program writes "*". This verifies bss is initialized.


### Configuring for your board

If you're using the ST-LINK connection and the USART via that USB device, then 
make sure the Makefile has the UART set to 2 (because the STLINK uses USART2):

```
UART_CHOICE := 2
```

If you're using the USART6 via the pins on the GPIO Expander, then set it to 6:
```
UART_CHOICE := 6
```
