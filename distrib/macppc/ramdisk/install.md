#	$OpenBSD: install.md,v 1.48 2011/01/03 00:36:49 deraadt Exp $
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

MDXAPERTURE=2
NCPU=$(sysctl -n hw.ncpufound)

((NCPU > 1)) && { DEFAULTSETS="bsd bsd.rd bsd.mp" ; SANESETS="bsd bsd.mp" ; }

md_installboot() {
	local _disk=$1

	if [[ -f /mnt/bsd.mp ]] && ((NCPU > 1)); then
		echo "Multiprocessor machine; using bsd.mp instead of bsd."
		mv /mnt/bsd /mnt/bsd.sp 2>/dev/null
		mv /mnt/bsd.mp /mnt/bsd
	fi

	# If there is an MSDOS partition on the boot disk, copy ofwboot
	# into it.
	if fdisk $_disk | grep -q 'Signature: 0xAA55'; then
		if fdisk $_disk | grep -q '^..: 06 '; then
			if mount /dev/${_disk}i /mnt2 >/dev/null 2>&1; then
				cp /usr/mdec/ofwboot /mnt2
				umount /mnt2
			fi
		fi
	fi
}

md_has_hfs () {
	pdisk -l /dev/$1c 2>&1 | grep -q "^Partition map "
}

md_has_hfs_openbsd () {
	pdisk -l /dev/$1c 2>&1 | grep -q " OpenBSD OpenBSD "
}

md_prep_MBR() {
	local _disk=$1 _q _d

	if md_has_hfs $_disk; then
		cat <<__EOT

WARNING: putting an MBR partition table on $_disk will DESTROY the existing HFS
         partitions and HFS partition table:
$(pdisk -l /dev/${_disk}c)

__EOT
		ask_yn "Are you *sure* you want an MBR partition table on $_disk?"
		[[ $resp == n ]] && return 1
	fi

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
			echo -n "Creating a 1MB DOS partition and an OpenBSD partition for rest of $_disk..."
			fdisk -e $_disk <<__EOT >/dev/null
reinit
update
write
quit
__EOT
			echo "done."
			break ;;
		e*|E*)
			# Manually configure the MBR.
			cat <<__EOT

You will now create one MBR partition to contain your OpenBSD data
and one MBR partition to contain the program that Open Firmware uses
to boot OpenBSD. Neither partition will overlap any other partition.

The OpenBSD MBR partition will have an id of 'A6' and the boot MBR
partition will have an id of '06' (DOS). The boot partition will be
at least 1MB and be marked as the *only* active partition.

$(fdisk $_disk)
__EOT
			fdisk -e $_disk
			fdisk $_disk | grep -q '^..: 06 ' || \
				{ echo "\nNo DOS (id 06) partition!\n" ; continue ; }
			fdisk $_disk | grep -q '^\*.: 06 ' || \
				{ echo "\nNo active DOS partition!\n" ; continue ; }
			fdisk $_disk | grep -q "^..: A6 " || \
				{ echo "\nNo OpenBSD (id A6) partition!\n" ; continue ; }
			break ;;
		o*|O*)	break ;;
		esac
	done

	disklabel $_disk 2>/dev/null | grep -q "^  i:" || disklabel -w -d $_disk
	newfs -t msdos ${_disk}i
}

md_prep_HFS() {
	local _disk=$1 _d _q
	
	while :; do
		_q=
		_d=Modify
		md_has_hfs_openbsd $_disk && \
			{ _q="Use the (O)penBSD partition, " ; _d=OpenBSD ; }
		pdisk -l /dev/${_disk}c
		ask "$_q(M)odify a partition or (A)bort?" "$_d"
		case $resp in
		a*|A*)	return 1 ;;
		o*|O*)	return 0 ;;
		m*|M*)	pdisk /dev/${_disk}c
			md_has_hfs_openbsd $_disk && break
			echo "\nNo 'OpenBSD'-type partition named 'OpenBSD'!"
		esac
	done

	return 0;
}

md_prep_disklabel() {
	local _disk=$1 _f _op

	PARTTABLE=
	while [[ -z $PARTTABLE ]]; do
		resp=MBR
		md_has_hfs $_disk && ask "Use HFS or MBR partition table?" HFS
		case $resp in
		m|mbr|M|MBR)
			md_prep_MBR $_disk || continue
			PARTTABLE=MBR
			;;
		h|hfs|H|HFS)
			md_prep_HFS $_disk || continue
			PARTTABLE=HFS
			;;
		esac
	done

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

You will now create an OpenBSD disklabel inside the OpenBSD $PARTTABLE
partition. The disklabel defines how OpenBSD splits up the $PARTTABLE partition
into OpenBSD partitions in which filesystems and swap space are created.
You must provide each filesystem's mountpoint in this program.

The offsets used in the disklabel are ABSOLUTE, i.e. relative to the
start of the disk, NOT the start of the OpenBSD $PARTTABLE partition.

__EOT

	disklabel -f $_f -E $_disk
}

md_congrats() {
	cat <<__EOT

INSTALL.$ARCH describes how to configure Open Firmware to boot OpenBSD. The
command to boot OpenBSD will be something like 'boot hd:,ofwboot /bsd'.
__EOT
	if [[ $PARTTABLE == HFS ]]; then
		cat <<__EOT

NOTE: You must use MacOS to copy 'ofwboot' from the OpenBSD install media to
the first HFS partition of $ROOTDISK.
__EOT
	fi

}

md_consoleinfo() {
	local _u _d=zstty

	for _u in $(scan_dmesg "/^$_d\([0-9]\) .*/s//\1/p"); do
		if [[ $_d$_u == $CONSOLE || -z $CONSOLE ]]; then
			CDEV=$_d$_u
			: ${CSPEED:=57600}
			set -- a b c d e f g h i j
			shift $_u
			CTTY=tty$1
			return
		fi
	done
}
