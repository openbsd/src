#	$OpenBSD: install.md,v 1.9 2002/08/27 02:18:34 krw Exp $
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
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#        This product includes software developed by the NetBSD
#        Foundation, Inc. and its contributors.
# 4. Neither the name of The NetBSD Foundation nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
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

# Machine-dependent install sets
MDSETS=kernel
MDFSTYPE=msdos
MDFSOPTS=-l
MDXAPERTURE=2
ARCH=ARCH

md_set_term() {
	local _tables

	ask "Do you wish to select a keyboard encoding table?" n

	case $resp in
	Y*|y*)	;;
	*)	return
		;;
	esac

	resp=
	while : ; do
		ask "Select your keyboard type: (P)C-AT/XT, (U)SB or 'done'" P
		case $resp in
		P*|p*)  _tables="be de dk es fr it jp lt no pt ru sf sg sv ua uk us"
			;;
		U*|u*)	_tables="de dk es fr it jp no sf sg sv uk us"
			;;
		done)	;;
		*)	echo "'$resp' is not a valid keyboard type."
			resp=
			continue
			;;
		esac
		break;
	done

	[ -z "$_tables" ] && return

	while : ; do
		cat << __EOT
The available keyboard encoding tables are:

	${_tables}

__EOT
		ask "Table name? (or 'done')" us
		case $resp in
		done)	;;
		*)	if kbd $resp ; then
				echo $resp > /tmp/kbdtype
			else
				echo "'${resp}' is not a valid table name."
				continue
			fi
			;;
		esac
		break;
	done
}

md_installboot() {
	echo Installing boot block...
	cp /usr/mdec/boot /mnt/boot
	/usr/mdec/installboot -v /mnt/boot /usr/mdec/biosboot ${1}
}

md_checkfordisklabel() {
	# $1 is the disk to check
	local rval

	disklabel -r $1 > /dev/null 2> /tmp/checkfordisklabel
	if grep "no disk label" /tmp/checkfordisklabel; then
		rval=1
	elif grep "disk label corrupted" /tmp/checkfordisklabel; then
		rval=2
	else
		rval=0
	fi

	rm -f /tmp/checkfordisklabel
	return $rval
}

md_prep_fdisk()
{
	local _disk=$1 _whole=$2

	if [ -n "$_whole" ]; then
		echo
		echo Updating MBR based on BIOS geometry.
		fdisk -e ${_disk} << __EOT
reinit
update
write
quit
__EOT

	else

		echo
		cat << __EOT
A single OpenBSD partition with id 'A6' ('OpenBSD') should exist in the MBR.
All of your OpenBSD partitions will be contained _within_ this partition,
including your swap space.  In the normal case it should be the only partition
marked as active.  (Unless you are using a multiple-OS booter, but you can
adjust that later.)  Furthermore, the MBR partitions must NOT overlap each
other.  [If this is a new install, you are most likely going to want to type
the following fdisk commands: reinit, update, write, quit. Use the 'manual'
command to read a full description.]  The current partition information is:

__EOT
		fdisk ${_disk}
		echo
		fdisk -e ${_disk}
	fi

	echo Here is the partition information you chose:
	echo
	fdisk ${_disk}
	echo
}

md_prep_disklabel()
{
	local _disk=$1

	ask "Do you want to use the *entire* disk for OpenBSD?" no
	case $resp in
	y*|Y*)	md_prep_fdisk ${_disk} Y ;;
	*)	md_prep_fdisk ${_disk} ;;
	esac

	cat << __EOT

Inside the BIOS 'A6' ('OpenBSD') partition you just created, there resides an
OpenBSD partition table which defines how this BIOS partition is to be split
up. This table declares the offsets and sizes of your / partition, your swap
space, and any other partitions you might create.  (NOTE: The OpenBSD disk
label offsets are absolute, ie. relative to the start of the disk... NOT
relative to the start of the BIOS 'A6' partition).

__EOT

	md_checkfordisklabel $_disk
	case $? in
	0)	;;
	1)	echo WARNING: Disk $_disk has no label. You will be creating a new one.
		echo
		;;
	2)	echo WARNING: Label on disk $_disk is corrupted. You will be repairing.
		echo
		;;
	esac

	# display example
	cat << __EOT
If this disk is shared with other operating systems, those operating systems
should have a BIOS partition entry that spans the space they occupy completely.
For safety, also make sure all OpenBSD file systems are within the offset and
size specified in the 'A6' BIOS partition table.  (By default, the disklabel
editor will try to enforce this).

__EOT
	disklabel -f /tmp/fstab.${_disk} -E ${_disk}
}

md_congrats() {
}
