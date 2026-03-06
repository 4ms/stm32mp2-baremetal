# SPDX-License-Identifier: GPL-2.0+

obj-$(CONFIG_USB_DWC3)			+= dwc3.o

dwc3-y					:= core.o

obj-$(CONFIG_USB_DWC3_GADGET)		+= gadget.o ep0.o

obj-$(CONFIG_$(SPL_)USB_DWC3_GENERIC)		+= dwc3-generic.o
