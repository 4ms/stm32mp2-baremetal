/*
 * usb_msc.h — Bare-metal USB Mass Storage host driver
 *
 * Implements the USB Mass Storage class, Bulk-Only Transport (BOT):
 *   bInterfaceClass    = 0x08 (Mass Storage)
 *   bInterfaceSubClass = 0x06 (SCSI transparent command set)
 *   bInterfaceProtocol = 0x50 (Bulk-Only)
 *
 * BOT wraps SCSI commands in a Command Block Wrapper (CBW) sent on the bulk
 * OUT endpoint, an optional data phase on bulk IN/OUT, and a Command Status
 * Wrapper (CSW) read from bulk IN. This driver issues the small read-only
 * subset of SCSI commands needed to identify a drive and read blocks.
 */

#ifndef USB_MSC_H
#define USB_MSC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct usb_device;

/*
 * usb_msc_init - Bind to a USB device's Mass Storage (BOT/SCSI) interface.
 *
 * Searches the config descriptor for a Mass-Storage / SCSI / Bulk-Only
 * interface, finds its bulk IN/OUT endpoints, and issues Get Max LUN.
 *
 * Returns 0 on success, negative on error (not an MSC device, etc.)
 */
int usb_msc_init(struct usb_device *dev);

/* Clear driver state on device removal. */
void usb_msc_disconnect(void);

/* True if a mass-storage device is currently bound. */
bool usb_msc_is_connected(void);

/*
 * usb_msc_inquiry - SCSI INQUIRY.
 *
 * @vendor:  buffer >= 9 bytes, filled with a NUL-terminated vendor string
 * @product: buffer >= 17 bytes, filled with a NUL-terminated product string
 * @device_type: SCSI peripheral device type (0 = direct-access / disk)
 * Returns 0 on success, negative on error.
 */
int usb_msc_inquiry(char *vendor, char *product, uint8_t *device_type);

/*
 * usb_msc_test_unit_ready - SCSI TEST UNIT READY.
 * Returns 0 if the medium is ready, 1 if not ready (check condition),
 * negative on transport error.
 */
int usb_msc_test_unit_ready(void);

/*
 * usb_msc_read_capacity - SCSI READ CAPACITY(10).
 * @last_lba:   receives the address of the last block (block count - 1)
 * @block_size: receives the block size in bytes
 * Returns 0 on success, negative on error.
 */
int usb_msc_read_capacity(uint32_t *last_lba, uint32_t *block_size);

/*
 * usb_msc_read_blocks - SCSI READ(10).
 * @lba:    first logical block address
 * @count:  number of blocks to read
 * @buf:    destination buffer (count * block_size bytes, 64-byte aligned)
 * Returns 0 on success, negative on error.
 */
int usb_msc_read_blocks(uint32_t lba, uint32_t count, void *buf);

#ifdef __cplusplus
}
#endif

#endif /* USB_MSC_H */
