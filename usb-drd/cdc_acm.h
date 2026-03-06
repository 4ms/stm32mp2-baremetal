#ifndef CDC_ACM_H
#define CDC_ACM_H

#include <linux/usb/gadget.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize CDC-ACM gadget on the given controller.
 * Call after dwc3_uboot_init() succeeds.
 * Returns 0 on success. */
int cdc_acm_init(struct usb_gadget *gadget);

/* True once host has sent SET_CONFIGURATION */
bool cdc_acm_is_configured(void);

/* Write data to host (bulk IN).
 * Returns bytes queued, 0 if TX busy, negative on error. */
int cdc_acm_write(const void *buf, int len);

/* Read data from host (bulk OUT).
 * Returns bytes copied, 0 if no data available. */
int cdc_acm_read(void *buf, int max_len);

#ifdef __cplusplus
}
#endif

#endif /* CDC_ACM_H */
