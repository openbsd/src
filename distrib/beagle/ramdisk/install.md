#	$OpenBSD: install.md,v 1.8 2013/07/30 02:49:54 bmercer Exp $
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

md_installboot() {
	local _disk=$1
	mount /dev/${_disk}i /mnt/mnt
	/mnt/usr/sbin/chroot /mnt /usr/sbin/mkuboot -a arm -o linux \
		-e 0x80300000 -l 0x80300000 /bsd /mnt/bsd.umg
	cp -r /tmp/u-boots/* /mnt/usr/mdec/
	BEAGLE=$(scan_dmesg '/^omap0 at mainbus0: \(BeagleBoard\).*/s//\1/p')
	BEAGLEBONE=$(scan_dmesg '/^omap0 at mainbus0: \(BeagleBone\).*/s//\1/p')
	PANDA=$(scan_dmesg '/^omap0 at mainbus0: \(PandaBoard\)/s//\1/p')
	if [[ -n $BEAGLE ]]; then
		cp /mnt/usr/mdec/beagle/{mlo,u-boot.bin} /mnt/mnt/
	elif [[ -n $BEAGLEBONE ]]; then
		cp /mnt/usr/mdec/am335x/{mlo,u-boot.img} /mnt/mnt/
	elif [[ -n $PANDA ]]; then
		cp /mnt/usr/mdec/panda/{mlo,u-boot.bin} /mnt/mnt/
	fi
	cat > /mnt/mnt/uenv.txt<<__EOT
bootcmd=mmc rescan ; setenv loadaddr 0x82800000 ; setenv bootargs sd0i:/bsd.umg ; fatload mmc 0 \${loadaddr} bsd.umg ; bootm \${loadaddr} ;
uenvcmd=boot
__EOT
}

md_prep_fdisk() {
	local _disk=$1 _q _d

	mount /dev/sd0i /mnt2
	cp -r /mnt2/u-boots/ /tmp/
	umount /mnt2
	while :; do
		_d=whole
		if [[ -n $(fdisk $_disk | grep 'Signature: 0xAA55') ]]; then
			fdisk $_disk
		else
			echo "MBR has invalid signature; not showing it."
		fi
		ask "Use (W)hole disk$_q or (E)dit the MBR?" "$_d"
		case $resp in
		w*|W*)
			echo -n "Creating a FAT partition and an OpenBSD partition for rest of $_disk..."
			fdisk -e ${_disk} <<__EOT >/dev/null
reinit
e 0
C
n
64
32768
f 0
e 3
A6
n
32832

write
quit
__EOT
			echo "done."
			disklabel $_disk 2>/dev/null | grep -q "^  i:" || disklabel -w -d $_disk
			newfs -t msdos ${_disk}i
			return ;;
		e*|E*)
			# Manually configure the MBR.
			cat <<__EOT

You will now create one MBR partition to contain your OpenBSD data
and one MBR partition on which kernels are located which are loaded
by U-Boot. Neither partition will overlap any other partition.

The OpenBSD MBR partition will have an id of 'A6' and the boot MBR
partition will have an id of 'C' (MSDOS). The boot partition will be
at least 16MB and be the first 'MSDOS' partition on the disk.

$(fdisk ${_disk})
__EOT
			fdisk -e ${_disk}
			[[ -n $(fdisk $_disk | grep ' A6 ') ]] && return
			echo No OpenBSD partition in MBR, try again. ;;
		o*|O*)	return ;;
		esac
	done
}

md_prep_disklabel() {
	local _disk=$1 _f _op

	md_prep_fdisk $_disk

	_f=/tmp/fstab.$_disk
	if [[ $_disk == $ROOTDISK ]]; then
		while :; do
			echo "The auto-allocated layout for $_disk is:"
			disklabel -h -A $_disk | egrep "^#  |^  [a-p]:"
			ask "Use (A)uto layout, (E)dit auto layout, or create (C)ustom layout?" a
			case $resp in
			a*|A*)	_op=-w ; AUTOROOT=y ;;
			e*|E*)	_op=-E ;;
			c*|C*)	break ;;
			*)	continue ;;
			esac
			disklabel $FSTABFLAG $_f $_op -A $_disk
			return
		done
	fi

	cat <<__EOT

You will now create an OpenBSD disklabel inside the OpenBSD MBR
partition. The disklabel defines how OpenBSD splits up the MBR partition
into OpenBSD partitions in which filesystems and swap space are created.
You must provide each filesystem's mountpoint in this program.

The offsets used in the disklabel are ABSOLUTE, i.e. relative to the
start of the disk, NOT the start of the OpenBSD MBR partition.

__EOT

	disklabel $FSTABFLAG $_f -E $_disk
}

md_congrats() {
}

md_consoleinfo() {
}
