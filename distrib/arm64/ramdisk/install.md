#	$OpenBSD: install.md,v 1.11 2018/10/12 18:37:22 kettenis Exp $
#
#
# Copyright (c) 1996 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Jason R. Thorpe.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
#
# machine dependent section of installation/upgrade script.
#

NCPU=$(sysctl -n hw.ncpufound)
NEWFSARGS_msdos="-F 16 -L boot"
MOUNT_ARGS_msdos="-o-l"

md_installboot() {
	local _disk=/dev/$1 _mdec _plat

	case $(sysctl -n hw.product) in
	*Pine64*)			_plat=pine64;;
	*'Raspberry Pi'*)		_plat=rpi;;
	esac

	# Mount MSDOS partition, extract U-Boot and copy UEFI boot program
	mount ${MOUNT_ARGS_msdos} ${_disk}i /mnt/mnt
	mkdir -p /mnt/mnt/efi/boot
	cp /mnt/usr/mdec/BOOTAA64.EFI /mnt/mnt/efi/boot/bootaa64.efi
	echo bootaa64.efi > /mnt/mnt/efi/boot/startup.nsh

	_mdec=/usr/mdec/$_plat

	case $_plat in
	pine64)
		dd if=$_mdec/u-boot-sunxi-with-spl.bin of=${_disk}c \
		    bs=1024 seek=8 >/dev/null 2>&1
		;;
	rpi)
		cp $_mdec/{bootcode.bin,start.elf,fixup.dat,*.dtb} /mnt/mnt/
		cp $_mdec/u-boot.bin /mnt/mnt/
		cat > /mnt/mnt/config.txt<<-__EOT
			arm_64bit=1
			enable_uart=1
			device_tree_address=0x02600000
			kernel=u-boot.bin
		__EOT
		;;
	esac
}

md_prep_fdisk() {
	local _disk=$1 _d

	local bootparttype="C"
	local bootsectorstart="8192"
	local bootsectorsize="32768"
	local bootsectorend=$(($bootsectorstart + $bootsectorsize))
	local bootfstype="msdos"
	local newfs_args=${NEWFSARGS_msdos}

	while :; do
		_d=whole
		if disk_has $_disk mbr; then
			fdisk $_disk
		else
			echo "MBR has invalid signature; not showing it."
		fi
		ask "Use (W)hole disk or (E)dit the MBR?" "$_d"
		case $resp in
		[wW]*)
			echo -n "Creating a ${bootfstype} partition and an OpenBSD partition for rest of $_disk..."
			fdisk -e ${_disk} <<__EOT >/dev/null
reinit
e 0
${bootparttype}
n
${bootsectorstart}
${bootsectorsize}
f 0
e 3
A6
n
${bootsectorend}

write
quit
__EOT
			echo "done."
			disklabel $_disk 2>/dev/null | grep -q "^  i:" || disklabel -w -d $_disk
			newfs -t ${bootfstype} ${newfs_args} ${_disk}i
			return ;;
		[eE]*)
			# Manually configure the MBR.
			cat <<__EOT

You will now create one MBR partition to contain your OpenBSD data
and one MBR partition on which the OpenBSD boot program is located.
Neither partition will overlap any other partition.

The OpenBSD MBR partition will have an id of 'A6' and the boot MBR
partition will have an id of '${bootparttype}' (${bootfstype}).
The boot partition will be at least 16MB and be the first 'MSDOS'
partition on the disk.

$(fdisk ${_disk})
__EOT
			fdisk -e ${_disk}
			disk_has $_disk mbr openbsd && return
			echo No OpenBSD partition in MBR, try again. ;;
		esac
	done
}

md_prep_disklabel() {
	local _disk=$1 _f=/tmp/i/fstab.$1

	md_prep_fdisk $_disk

	disklabel_autolayout $_disk $_f || return
	[[ -s $_f ]] && return

	# Edit disklabel manually.
	# Abandon all hope, ye who enter here.
	disklabel -F $_f -E $_disk
}

md_congrats() {
}

md_consoleinfo() {
	CTTY=console
	DEFCONS=y
	case $CSPEED in
	9600|19200|38400|57600|115200|1500000)
		;;
	*)
		CSPEED=115200;;
	esac
}
