/*
 * usb_midi.c — Bare-metal USB MIDI host driver
 */

#include "usb_midi.h"
#include <common.h>
#include <usb.h>
#include <string.h>

/* USB MIDI Streaming subclass (USB_CLASS_AUDIO is in usb_defs.h) */
#define USB_SUBCLASS_MIDISTREAMING 0x03

static struct usb_device *midi_dev;
static u8 ep_in;	/* bulk IN endpoint address (0x8x) */
static u8 ep_out;	/* bulk OUT endpoint address (0x0x) */
static u16 ep_in_maxpkt;
static u16 ep_out_maxpkt;

int usb_midi_init(struct usb_device *dev)
{
	midi_dev = NULL;
	ep_in = 0;
	ep_out = 0;

	/* Search for Audio/MIDI Streaming interface */
	for (int i = 0; i < dev->config.no_of_if; i++) {
		struct usb_interface *iface = &dev->config.if_desc[i];
		if (iface->desc.bInterfaceClass == USB_CLASS_AUDIO &&
		    iface->desc.bInterfaceSubClass == USB_SUBCLASS_MIDISTREAMING) {

			/* Find bulk IN and OUT endpoints */
			for (int e = 0; e < iface->no_of_ep; e++) {
				struct usb_endpoint_descriptor *ep =
					&iface->ep_desc[e];
				u8 xfer_type = ep->bmAttributes & 0x03;
				if (xfer_type != USB_ENDPOINT_XFER_BULK)
					continue;

				if ((ep->bEndpointAddress & USB_DIR_IN) && !ep_in) {
					ep_in = ep->bEndpointAddress;
					ep_in_maxpkt = le16_to_cpu(ep->wMaxPacketSize);
				} else if (!(ep->bEndpointAddress & USB_DIR_IN) && !ep_out) {
					ep_out = ep->bEndpointAddress;
					ep_out_maxpkt = le16_to_cpu(ep->wMaxPacketSize);
				}
			}

			if (ep_in || ep_out) {
				midi_dev = dev;
				printf("USB MIDI: bound to interface %d\n", i);
				if (ep_in)
					printf("  EP IN  0x%02x (maxpkt %d)\n",
					       ep_in, ep_in_maxpkt);
				if (ep_out)
					printf("  EP OUT 0x%02x (maxpkt %d)\n",
					       ep_out, ep_out_maxpkt);
				return 0;
			}
		}
	}

	printf("USB MIDI: no MIDI Streaming interface found\n");
	return -ENODEV;
}

void usb_midi_disconnect(void)
{
	midi_dev = NULL;
	ep_in = 0;
	ep_out = 0;
	ep_in_maxpkt = 0;
	ep_out_maxpkt = 0;
}

bool usb_midi_is_connected(void)
{
	return midi_dev != NULL;
}

int usb_midi_read(struct usb_midi_event *events, int max_events)
{
	if (!midi_dev || !ep_in)
		return -ENODEV;

	int buf_size = max_events * 4;
	if (buf_size > 64)
		buf_size = 64;

	unsigned char buf[64] __attribute__((aligned(64)));
	unsigned long pipe = usb_rcvbulkpipe(midi_dev,
					     ep_in & USB_ENDPOINT_NUMBER_MASK);

	int ret = submit_bulk_msg(midi_dev, pipe, buf, buf_size);
	if (ret == -ETIMEDOUT || ret == -110)
		return 0; /* no data available, not an error */
	if (ret < 0)
		return ret;

	int actual = midi_dev->act_len;
	int n_events = actual / 4;
	if (n_events > max_events)
		n_events = max_events;

	for (int i = 0; i < n_events; i++) {
		events[i].header = buf[i * 4 + 0];
		events[i].midi[0] = buf[i * 4 + 1];
		events[i].midi[1] = buf[i * 4 + 2];
		events[i].midi[2] = buf[i * 4 + 3];
	}

	return n_events;
}

int usb_midi_write(const struct usb_midi_event *events, int num_events)
{
	if (!midi_dev || !ep_out)
		return -ENODEV;

	int buf_size = num_events * 4;
	if (buf_size > 64)
		buf_size = 64;

	unsigned char buf[64] __attribute__((aligned(64)));
	for (int i = 0; i < num_events; i++) {
		buf[i * 4 + 0] = events[i].header;
		buf[i * 4 + 1] = events[i].midi[0];
		buf[i * 4 + 2] = events[i].midi[1];
		buf[i * 4 + 3] = events[i].midi[2];
	}

	unsigned long pipe = usb_sndbulkpipe(midi_dev,
					     ep_out & USB_ENDPOINT_NUMBER_MASK);

	int ret = submit_bulk_msg(midi_dev, pipe, buf, buf_size);
	if (ret < 0)
		return ret;

	return 0;
}
