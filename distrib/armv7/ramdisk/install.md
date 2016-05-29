#	$OpenBSD: install.md,v 1.35 2016/05/29 15:10:05 jsg Exp $
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

	# Identify ARMv7 platform based on dmesg.
	case $(scan_dmesg 's/^mainbus0 at root: \(.*\)$/\1/p') in
	*AM335x*)			_plat=am335x;;
	*"OMAP3 BeagleBoard"*)		_plat=beagle;;
	*OMAP4*)			_plat=panda;;
	*Cubieboard*)			_plat=cubie;;
	*Cubox-i*|*HummingBoard*)	_plat=cubox;;
	*Wandboard*)			_plat=wandboard;;
	*Nitrogen6*|*"SABRE Lite"*)	_plat=nitrogen;;
	*)				;; # XXX: Handle unknown platform?
	esac

	# Mount MSDOS partition, extract U-Boot and copy UEFI boot program
	mount ${MOUNT_ARGS_msdos} /dev/${_disk}i /mnt/mnt
	tar -C /mnt/ -xf /usr/mdec/u-boots.tgz 
	mkdir -p /mnt/mnt/efi/boot
	cp /mnt/usr/mdec/BOOTARM.EFI /mnt/mnt/efi/boot/bootarm.efi

	_mdec=/mnt/usr/mdec/$_plat

	case $_plat in
	am335x|beagle|panda)
		cp $_mdec/{MLO,u-boot.img,*.dtb} /mnt/mnt/
		;;
	cubox|wandboard)
		cp $_mdec/*.dtb /mnt/mnt/
		dd if=$_mdec/SPL of=${_disk}c bs=1024 seek=1 >/dev/null
		dd if=$_mdec/u-boot.img of=${_disk}c bs=1024 seek=69 >/dev/null
		;;
	nitrogen)
		cp $_mdec/*.dtb /mnt/mnt/
		;;
	cubie)
		cp $_mdec/{u-boot-sunxi-with-spl.bin,*.dtb} /mnt/mnt/
		;;
	esac
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
