/*
 * usb_midi.h — Bare-metal USB MIDI host driver
 *
 * USB MIDI devices use Audio class (0x01), MIDI Streaming subclass (0x03).
 * Data is transferred in 4-byte USB-MIDI Event Packets over bulk endpoints.
 *
 * USB-MIDI Event Packet format (4 bytes):
 *   Byte 0: Cable Number (hi nibble) | Code Index Number (lo nibble)
 *   Byte 1: MIDI byte 1 (status)
 *   Byte 2: MIDI byte 2 (data1)
 *   Byte 3: MIDI byte 3 (data2)
 */

#ifndef USB_MIDI_H
#define USB_MIDI_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct usb_device;

/* USB-MIDI Event Packet */
struct usb_midi_event {
	uint8_t header;  /* cable_num << 4 | code_index */
	uint8_t midi[3]; /* MIDI message bytes */
};

/*
 * usb_midi_init - Bind to a USB device's MIDI interface
 *
 * Searches the device's config descriptor for an Audio/MIDI Streaming
 * interface and finds the bulk IN/OUT endpoints.
 *
 * Returns 0 on success, negative on error (not a MIDI device, etc.)
 */
int usb_midi_init(struct usb_device *dev);

/*
 * usb_midi_read - Read MIDI events from the device (non-blocking)
 *
 * Submits a bulk IN transfer and returns the number of 4-byte
 * USB-MIDI Event Packets read, or 0 if no data, negative on error.
 *
 * @events: buffer to receive events
 * @max_events: maximum number of events to read
 */
int usb_midi_read(struct usb_midi_event *events, int max_events);

/*
 * usb_midi_write - Send MIDI events to the device
 *
 * @events: events to send
 * @num_events: number of events
 * Returns 0 on success, negative on error.
 */
int usb_midi_write(const struct usb_midi_event *events, int num_events);

/*
 * usb_midi_is_connected - Check if a MIDI device is bound
 */
bool usb_midi_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_MIDI_H */
