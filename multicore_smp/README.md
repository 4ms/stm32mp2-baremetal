## Multi-core SMP demo

This demonstrates launching the second CA35 core to execute at a specified address.

It uses TF-A SMC to do this (which reports log messages starting with "I/TC:")

Then, each core prints T1ck or T0ck and sends a SGI interrupt to the other core.

FIXME: Core1 sending SGI6 to Core0 does not work.

With the FIXME not fixed, you should see this:

```
Multicore A35 Test
I/TC: Secondary CPU 1 initializing
I/TC: Secondary CPU 1 switching to normal world boot
System call to start aux CA35 core returned 0 (0=success)
Aux is online
Aux enabled SGI
T1ck
T0ck
> Core1 SGI1 received
T1ck
T0ck
> Core1 SGI1 received
T1ck
T0ck
> Core1 SGI1 received
T1ck
T0ck
...
```
