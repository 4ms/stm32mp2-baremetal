---
name: TCPP03 ACK register readback required
description: TCPP03-M20 requires reading ACK register (reg1) after writing reg0 or commands don't take effect. A delay alone is not sufficient.
type: feedback
---

The TCPP03-M20 on the EV1 board requires reading back the ACK register (register 1) after writing to the programming control register (register 0). Without this readback, VBUS does not go high even with a long delay substituted. The readback likely serves as a command handshake/synchronization mechanism. Always read reg1 after writing reg0.
