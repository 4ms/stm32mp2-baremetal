#ifndef _DWC3_UBOOT_H
#define _DWC3_UBOOT_H

/* Minimal stub — U-Boot's dwc3-uboot.h provides the glue interface.
 * For bare-metal, we define only what the driver references. */

struct dwc3;

int dwc3_uboot_init(struct dwc3 *dwc);
void dwc3_uboot_exit(int index);

#endif /* _DWC3_UBOOT_H */
