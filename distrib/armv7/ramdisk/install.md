#	$OpenBSD: install.md,v 1.34 2016/05/29 07:38:01 jsg Exp $
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

SANESETS="bsd"
DEFAULTSETS="bsd bsd.rd"

NEWFSARGS_msdos="-F 16 -L boot"
MOUNT_ARGS_msdos="-o-l"

md_installboot() {
	local _disk=$1
	local mount_args=${MOUNT_ARGS_msdos}

	BEAGLE=$(scan_dmesg '/^omap0 at mainbus0: TI OMAP3 \(BeagleBoard\).*/s//\1/p')
	BEAGLEBONE=$(scan_dmesg '/^omap0 at mainbus0: TI AM335x \(BeagleBone\).*/s//\1/p')
	PANDA=$(scan_dmesg '/^omap0 at mainbus0: TI OMAP4 \(PandaBoard\)/s//\1/p')
	CUBOX=$(scan_dmesg '/^imx0 at mainbus0: \(SolidRun.*\)/s//CUBOX/p')
	NITROGEN=$(scan_dmesg '/^imx0 at mainbus0: \(Freescale i.MX6 SABRE Lite.*\)/s//NITROGEN/p')
	WANDBOARD=$(scan_dmesg '/^imx0 at mainbus0: \(Wandboard i.MX6.*\)/s//WANDBOARD/p')
	CUBIE=$(scan_dmesg '/^mainbus0 at root: Cubietech \(Cubieboard\)/s//CUBIEBOARD/p')

	mount ${mount_args} /dev/${_disk}i /mnt/mnt

	# extracted on all machines, so make snap works.
	tar -C /mnt/ -xf /usr/mdec/u-boots.tgz 

	mkdir -p /mnt/mnt/efi/boot
	cp /mnt/usr/mdec/BOOTARM.EFI /mnt/mnt/efi/boot/bootarm.efi

	if [[ -n $BEAGLE ]]; then
		cp /mnt/usr/mdec/beagle/{MLO,u-boot.img} /mnt/mnt/
		cp /mnt/usr/mdec/beagle/*.dtb /mnt/mnt/
	elif [[ -n $BEAGLEBONE ]]; then
		cp /mnt/usr/mdec/am335x/{MLO,u-boot.img} /mnt/mnt/
		cp /mnt/usr/mdec/am335x/*.dtb /mnt/mnt/
	elif [[ -n $PANDA ]]; then
		cp /mnt/usr/mdec/panda/{MLO,u-boot.img} /mnt/mnt/
		cp /mnt/usr/mdec/panda/*.dtb /mnt/mnt/
	elif [[ -n $CUBOX ]]; then
		cp /mnt/usr/mdec/cubox/*.dtb /mnt/mnt/
		dd if=/mnt/usr/mdec/cubox/SPL \
		    of=/dev/${_disk}c bs=1024 seek=1 >/dev/null
		dd if=/mnt/usr/mdec/cubox/u-boot.img \
		    of=/dev/${_disk}c bs=1024 seek=69 >/dev/null
	elif [[ -n $NITROGEN ]]; then
		cp /mnt/usr/mdec/nitrogen/*.dtb /mnt/mnt/
	elif [[ -n $WANDBOARD ]]; then
		cp /mnt/usr/mdec/wandboard/*.dtb /mnt/mnt/
		dd if=/mnt/usr/mdec/wandboard/SPL \
		    of=/dev/${_disk}c bs=1024 seek=1 >/dev/null
		dd if=/mnt/usr/mdec/wandboard/u-boot.img \
		    of=/dev/${_disk}c bs=1024 seek=69 >/dev/null
	elif [[ -n $CUBIE ]]; then
		cp /mnt/usr/mdec/cubie/u-boot-sunxi-with-spl.bin /mnt/mnt/
		cp /mnt/usr/mdec/cubie/*.dtb /mnt/mnt/
	fi
}

md_prep_fdisk() {
	local _disk=$1 _d

	local bootparttype="C"
	local bootsectorstart="2048"
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
		ask "Use (W)hole disk$ or (E)dit the MBR?" "$_d"
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
and one MBR partition on which kernels are located which are loaded
by U-Boot. Neither partition will overlap any other partition.

The OpenBSD MBR partition will have an id of 'A6' and the boot MBR
partition will have an id of '${bootparttype}' (${bootfstype}). The boot partition will be
at least 16MB and be the first 'MSDOS' partition on the disk.

$(fdisk ${_disk})
__EOT
			fdisk -e ${_disk}
			disk_has $_disk mbr openbsd && return
			echo No OpenBSD partition in MBR, try again. ;;
		esac
	done
}

md_prep_disklabel() {
	local _disk=$1 _f=/tmp/fstab.$1

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
}
