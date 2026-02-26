#!/usr/bin/env bash

# Script to format and partition an SD card to prepare for bootloaders and a bare-metal app.
#
# This script is meant to be run once when using a new SD Card.
 

[ "$#" -eq 1 ] || { 
	echo ""
	echo "Usage: "
	echo "scripts/partition-sdcard.sh /dev/XXX"  >&2; 
	echo ""
	echo "Where /dev/XXX is the sd card device, e.g. /dev/sdc or /dev/disk3"
	exit 1; 
}

if [ ! -b $1 ]; then
	echo "Device $1 does not exist";
	exit 1;
fi

# On mac, must erase disk first
echo ""
case "$(uname -s)" in
	Darwin)
		echo "Formatting"
		set -x
		diskutil eraseDisk FAT32 TMPDISK $1
		diskutil unmountDisk $1
		set +x
		;;
	*)
		;;
esac

echo ""
echo "Clearing partition table and converting MBR to GPT if present..."
echo ""

echo "sudo sgdisk -Z $1"
sudo sgdisk -go $1  || exit

echo ""#
echo "Partitioning..."
set -x
sudo sgdisk --resize-table=128 -a 1 \
	-n 1:34:545 		-c 1:fsbla1 	-t 1:8DA63339-0007-60C0-C436-083AC8230908 \
	-n 2:546:1057 		-c 2:fsbla2 	-t 2:8DA63339-0007-60C0-C436-083AC8230908 \
	-n 3:1058:1569 		-c 3:metadata1 	-t 3:8A7A84A0-8387-40F6-AB41-A8B9A5A60D23 \
	-n 4:1570:2081 		-c 4:metadata2 	-t 4:8A7A84A0-8387-40F6-AB41-A8B9A5A60D23 \
	-n 5:2082:10273 	-c 5:fip-a 		-t 5:19D5DF83-11B0-457B-BE2C-7559C13142A5 \
	-n 6:10274:18465 	-c 6:fip-b 		-t 6:19D5DF83-11B0-457B-BE2C-7559C13142A5 \
	-n 7:18466:19489 	-c 7:u-boot-env -t 7:3DE21764-95BD-54BD-A5C3-4ABE786F38A8 \
	-n 8:19490:150562 	-c 8:bootfs 	-t 8:0FC63DAF-8483-4772-8E79-3D69D8477DE4 \
	-N 9 				-c 9:fatfs 		-t 9:EBD0A0A2-B9E5-4433-87C0-68B6B72699C7 \
	-p $1 || exit
set +x

echo ""
echo "Formatting partition 9 as FAT32"

echo ""
case "$(uname -s)" in
	Darwin)
		set -x
		diskutil eraseVolume FAT32 BAREAPP ${1}s9 || exit
		sleep 1
		diskutil unmountDisk $1
		set +x
		;;
	Linux)
		read -p "You must eject and re-insert the SD Card now. Press enter when ready." READY
		set -x
		sudo umount ${1}9
		sudo mkfs.fat -F 32 -n BAREAPP ${1}9 || exit
		set +x
		;;
	*)
		echo 'OS not supported: please format $1 partition 9 as FAT32'
		;;
esac

echo "Success!"

