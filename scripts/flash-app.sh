#!/usr/bin/env bash

# Copy an app image (main.uimg) onto the SD card partition named "app",
# where TF-A's baremetal loader looks for it on boot.
#
# Usage: scripts/flash-app.sh /dev/XXX path/to/main.uimg
#
# Where /dev/XXX is the whole SD card device (e.g. /dev/disk4 or /dev/sdc).
# The "app" partition is located by its GPT name, so the partition number
# doesn't matter (scripts/partition-sdcard.sh creates it as partition 9).

set -e

[ "$#" -eq 2 ] || {
	echo ""
	echo "Usage:"
	echo "scripts/flash-app.sh /dev/XXX path/to/main.uimg" >&2
	echo ""
	echo "Where /dev/XXX is the sd card device, e.g. /dev/sdc or /dev/disk4"
	exit 1
}

DEV=$1
UIMG=$2

if [ ! -e "$DEV" ]; then
	echo "Device $DEV does not exist"
	exit 1
fi

if [ ! -f "$UIMG" ]; then
	echo "Image file $UIMG does not exist (build it with 'make')"
	exit 1
fi

# Find the partition number whose GPT name is "app"
PART_NUM=$(sudo sgdisk -p "$DEV" | awk '$NF == "app" {print $1}')
if [ -z "$PART_NUM" ]; then
	echo "No partition named 'app' found on $DEV."
	echo "Create one by re-running scripts/partition-sdcard.sh (erases the card!)"
	exit 1
fi

case "$(uname -s)" in
	Darwin)
		PART_DEV="${DEV}s${PART_NUM}"
		diskutil unmountDisk "$DEV" >/dev/null || true
		;;
	*)
		# /dev/sdc -> /dev/sdc9, but /dev/mmcblk0 -> /dev/mmcblk0p9
		case "$DEV" in
			*[0-9]) PART_DEV="${DEV}p${PART_NUM}" ;;
			*)      PART_DEV="${DEV}${PART_NUM}" ;;
		esac
		;;
esac

echo "Writing $UIMG to $PART_DEV (partition ${PART_NUM}: 'app')"
sudo dd if="$UIMG" of="$PART_DEV" bs=1m 2>/dev/null || sudo dd if="$UIMG" of="$PART_DEV" bs=1M
sync

echo "Done. Reset the board to run the new app."
