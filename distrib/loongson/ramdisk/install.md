#	$OpenBSD: install.md,v 1.3 2010/02/18 22:15:12 otto Exp $
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

	if mount -t ext2fs /dev/${_disk}i /mnt2 ; then
		if mkdir -p /mnt2/boot && cp /usr/mdec/boot /mnt2/boot; then
			umount /mnt2
			return
		fi
	fi

	echo "Failed to install bootblocks."
	echo "You will not be able to boot OpenBSD from $_disk."
	exit
}

md_prep_fdisk() {
	local _disk=$1 _q _d

	while :; do
		_d=whole
		if fdisk $_disk | grep -q 'Signature: 0xAA55'; then
			fdisk $_disk
			if fdisk $_disk | grep -q '^..: A6 '; then
				_q=", use the (O)penBSD area,"
				_d=OpenBSD
			fi
		else
			echo "MBR has invalid signature; not showing it."
		fi
		ask "Use (W)hole disk$_q or (E)dit the MBR?" "$_d"
		case $resp in
		w*|W*)
			echo -n "Creating a 1MB ext2 partition and an OpenBSD partition for rest of $_disk..."
			fdisk -e $_disk <<__EOT >/dev/null
reinit
update
write
quit
__EOT
			echo "done."
			disklabel $_disk 2>/dev/null | grep -q "^  i:" || disklabel -w -d $_disk
			newfs -qt ext2fs ${_disk}i
			break ;;
		e*|E*)
			# Manually configure the MBR.
			cat <<__EOT

You will now create one MBR partition to contain your OpenBSD data
and one MBR partition to contain the program that PMON uses
to boot OpenBSD. Neither partition will overlap any other partition.

The OpenBSD MBR partition will have an id of 'A6' and the boot MBR
partition will have an id of '83' (Linux files). The boot partition will be
at least 1MB and be the first 'Linux files' partition on the disk.
The installer assumes there is already an ext2 or ext3 filesystem on the
first 'Linux files' partition.

$(fdisk ${_disk})
__EOT
			fdisk -e $_disk
			fdisk $_disk | grep -q '^..: 83 ' || \
				{ echo "\nNo Linux files (id 83) partition!\n" ; continue ; }
			fdisk $_disk | grep -q "^..: A6 " || \
				{ echo "\nNo OpenBSD (id A6) partition!\n" ; continue ; }
			disklabel $_disk 2>/dev/null | grep -q "^  i:" || disklabel -w -d $_disk
			break ;;
		o*|O*)	break ;;
		esac
	done

}

md_prep_disklabel() {
	local _disk=$1 _f _op

	md_prep_fdisk $_disk

	disklabel -W $_disk >/dev/null 2>&1
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
			disklabel -f $_f $_op -A $_disk
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

	disklabel -f $_f -E $_disk
}

md_congrats() {
	cat <<__EOT

Once the machine has rebooted use PMON to boot into OpenBSD, as
described in the INSTALL.$ARCH document. The command to boot the OpenBSD
bootloader will be something like 'boot /dev/fs/ext2@wd0/boot/boot'

__EOT
}

md_consoleinfo() {
}
