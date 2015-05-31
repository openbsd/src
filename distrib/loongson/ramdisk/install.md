#	$OpenBSD: install.md,v 1.18 2015/05/31 19:40:10 rpe Exp $
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

	# Use cat below to avoid holes created by cp(1)
	if mount -t ext2fs /dev/${_disk}i /mnt2 &&
	   mkdir -p /mnt2/boot &&
	   cat /mnt/usr/mdec/boot > /mnt2/boot/boot &&
	   { [[ $(sysctl -n hw.product) != Gdium ]] ||
	     cp /mnt/bsd /mnt2/boot/bsd; }; then
		umount /mnt2
		return
	fi

	echo "Failed to install bootblocks."
	echo "You will not be able to boot OpenBSD from $_disk."
	exit
}

md_prep_fdisk() {
	local _disk=$1 _q _d _s _o

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
			if [ $(sysctl -n hw.product) = Gdium ]; then
				_s=32
				_o="-O 1 -b 4096"
			else
				_s=1
				_o=""
			fi
			echo -n "Creating a ${_s}MB ext2 partition and an OpenBSD partition for rest of $_disk..."
			fdisk -e $_disk <<__EOT >/dev/null
re
e 0
83

1
$((_s * 2048))
e 3
0
e 3
A6

$((_s * 2048 + 1))
*
update
write
quit
__EOT
			echo "done."
			disklabel $_disk 2>/dev/null | grep -q "^  i:" || disklabel -w -d $_disk
			newfs -qt ext2fs $_o ${_disk}i
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
				{ echo "\nNo Linux files (id 83) partition!\n"; continue; }
			fdisk $_disk | grep -q "^..: A6 " || \
				{ echo "\nNo OpenBSD (id A6) partition!\n"; continue; }
			disklabel $_disk 2>/dev/null | grep -q "^  i:" || disklabel -w -d $_disk
			break ;;
		o*|O*)	break ;;
		esac
	done

}

md_prep_disklabel() {
	local _disk=$1 _f=/tmp/fstab.$1

	md_prep_fdisk $_disk

	disklabel_autolayout $_disk $_f || return
	[[ -s $_f ]] && return

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
	cat <<__EOT

Once the machine has rebooted use PMON to boot into OpenBSD, as
described in the INSTALL.$ARCH document.
To load the OpenBSD bootloader, use 'boot /dev/fs/ext2@wd0/boot/boot',
where wd0 is the PMON name of the boot disk.

__EOT
}

md_consoleinfo() {
}
