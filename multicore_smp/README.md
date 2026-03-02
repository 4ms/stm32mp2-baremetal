## Multi-core SMP demo

This demonstrates launching the second CA35 core to execute at a specified address.

Some terminology:

The second CA35 core is called "Core1" in this example (as opposed to the main "Core0").
The reference manual calls it "Processor 1", and calls the main core "Processor 0".
BL31 Secure Monitor calls it "Secondary CPU 1", and sometimes in code I call it the "aux" core.
Both of these cores/processors are part of "CPU1".

When run from EL1 non-secure, the main core uses the PSCI protocol (via SMCCC) to send a message
to the Secure Monitor BL31 to launch the secondary core.
The message is sent by putting a command number (0xC4000003) into register x0, the core to wakeup
into register x1, and the execution address into register x2. Then an `smc` assembly instruction
is executed.
This triggers an interrupt which is handled by the Secure Monitor. You'll see console log messages
starting with "I/TC:" from the Secure Monitor as it boots the secondary core.

When run from EL3, we don't have the Secure Monitor running, so we need to
reset the core manually. This is done by writing the address to execute into a
register in the CA35SYSCFG registers, and the setting the C1P1
(CPU1:Processor1) reset bit in the RCC registers.

Once booted, each core prints `T1ck` or `T0ck` and sends an SGI interrupt to the other core.

In EL1, with SMC:
```
Multicore A35 Test
I/TC: Secondary CPU 1 initializing
I/TC: Secondary CPU 1 switching to normal world boot
System call to start aux CA35 core returned 0 (0=success)
Aux is online
Aux enabled SGI
T1ck
> Core0 SGI6 received
T0ck
> Core1 SGI1 received
T1ck
> Core0 SGI6 received
T0ck
> Core1 SGI1 received
T1ck
> Core0 SGI6 received
T0ck
...
```

With EL3:
```
Multicore A35 Test
System call to start aux CA35 core returned 0 (0=success)
Aux is online
Aux enabled SGI
T1ck
> Core0 SGI6 received
T0ck
> Core1 SGI1 received
T1ck
> Core0 SGI6 received
T0ck
> Core1 SGI1 received
T1ck
> Core0 SGI6 received
T0ck
...
```
