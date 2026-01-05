## C Runtime Test

This project clears the bss region, and has the link script setup so the static/global data section 
is initialized. It also sets up the stack. All these things are required for C (or C++) functions to work.

It also sets up the MMU, though this is not being demonstrated.


The output should be:

```
MP2
CurrentEL=0000000000000001
>XYZ
```


The "MP2" is printed just for reference that the UART is working.

The CurrentEL=00...1 tells us if we're in EL1 or EL2 (U-Boot/TFA can be configured either way).

The ">" will be "EL2>" if we started in EL2. The > is printed when we drop to EL1.

The "X" is a read-only global, which demonstrates read-only .data section is correct.

The "Y" is a read/write global, which demonstrates the .data section is correct.

The "Z" is a written if the zero-initialized global is actually 0. If it is non-zero,
then the program writes "*".
