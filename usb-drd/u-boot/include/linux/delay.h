#ifndef _LINUX_DELAY_H
#define _LINUX_DELAY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Busy-wait microseconds — implemented using AArch64 generic timer */
void udelay(unsigned int us);

/* Busy-wait milliseconds */
void mdelay(unsigned int ms);

#ifdef __cplusplus
}
#endif

#endif /* _LINUX_DELAY_H */
