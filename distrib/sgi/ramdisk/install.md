#	$OpenBSD: install.md,v 1.34 2015/05/04 19:55:26 rpe Exp $
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

IPARCH=$(sysctl -n hw.model)
NCPU=$(sysctl -n hw.ncpufound)

MDSETS="bsd.${IPARCH} bsd.rd.${IPARCH}"
SANESETS="bsd.${IPARCH}"
# Since we do not provide bsd.mp on IP27 yet, do not add bsd.mp.IP27 to the
# sets, as this will cause a warning in sane_install()
if ((NCPU > 1)) && [[ $IPARCH = IP30 ]]; then
	MDSETS="${MDSETS} bsd.mp.${IPARCH}"
	SANESETS="${SANESETS} bsd.mp.${IPARCH}"
fi
DEFAULTSETS=${MDSETS}

md_installboot() {
	local _disk=$1

	echo "Installing boot loader in volume header."
	/usr/mdec/sgivol -w boot /mnt/usr/mdec/boot-`sysctl -n hw.model` $_disk
	case $? in
	0)
		;;
	*)	echo
		echo "WARNING: Boot install failed. Booting from disk will not be possible"
		;;
	esac

	if [[ -f /mnt/bsd.${IPARCH} ]]; then
		mv /mnt/bsd.${IPARCH} /mnt/bsd
	fi
	if [[ -f /mnt/bsd.mp.${IPARCH} ]]; then
		mv /mnt/bsd.mp.${IPARCH} /mnt/bsd.mp
	fi
	if [[ -f /mnt/bsd.rd.${IPARCH} ]]; then
		mv /mnt/bsd.rd.${IPARCH} /mnt/bsd.rd
	fi
}

md_prep_disklabel()
{
	local _disk=$1 _f _op

	echo
	echo "Checking SGI Volume Header:"
	/usr/mdec/sgivol -q $_disk >/dev/null 2>/dev/null
	case $? in
	0)	/usr/mdec/sgivol $_disk
	cat <<__EOT

An SGI Volume Header was found on the disk. Normally you want to replace it
with a new Volume Header suitable for installing OpenBSD. Doing this will
of course delete all data currently on the disk.
__EOT
		ask "Do you want to overwrite the current header?" y
		case "$resp" in
		y*|Y*)
			/usr/mdec/sgivol -qi $_disk
			;;
		n*|N*)
			cat <<__EOT

If the Volume Header was installed by a previous OpenBSD install keeping
it is OK as long as the Volume Header has room for the 'boot' program.
If you are trying to keep an old IRIX Volume Header, OpenBSD install will
use the 'a' partition on the disk for the install and any data in that
partition will be lost.
__EOT
			ask "Are you sure you want to try to keep the old header?" y
			case "$resp" in
			y*|Y*)
				;;
			n*|N*)
				ask "Do you want to overwrite the old header instead?" n
				case "$resp" in
				y*|Y*)
					/usr/mdec/sgivol -qi $_disk
					;;
				n*|N*)
					return 1
					;;
				esac
				;;
			esac
			;;
		esac
		;;
	1)	echo
		echo "Your disk seems to be unaccessible. It was not possible"
		echo "determine if there is a proper Volume Header or not."
		ask "Do you want to continue anyway?" n
		case "$resp" in
		y*|Y*)
			;;
		n*|N*)
			return 1 ;;
		esac
		;;
	2)	echo
		echo "There is no Volume Header found on the disk. A Volume"
		echo "header is required to be able to boot from the disk."
		ask "Do you want to install a Volume Header?" y
		case "$resp" in
		y*|Y*)
			/usr/mdec/sgivol -qi $_disk
			;;
		n*|N*)
			return 1
			;;
		esac
		;;
	esac

	_f=/tmp/fstab.$_disk
	if [[ $_disk == $ROOTDISK ]]; then
		if $AUTO && get_disklabel_template; then
			disklabel -T /disklabel.auto $FSTABFLAG $_f -w -A $_disk && return
			echo "Autopartitioning failed"
			exit 1
		fi
		while :; do
			echo "The auto-allocated layout for $_disk is:"
			disklabel -h -A $_disk | egrep "^#  |^  [a-p]:"
			ask "Use (A)uto layout, (E)dit auto layout, or create (C)ustom layout?" a
			case $resp in
			a*|A*)	_op=-w ;;
			e*|E*)	_op=-E ;;
			c*|C*)	break ;;
			*)	continue ;;
			esac
			disklabel $FSTABFLAG $_f $_op -A $_disk
			return
		done
	fi

	cat <<__EOT

You will now create an OpenBSD disklabel. The disklabel must have an
'a' partition, being the space available for OpenBSD's root file system.
The 'p' partition must be retained since it contains the SGI Volume Header;
this in turn contains the boot loader. No other partitions should overlap
with the SGI Volume Header, which by default will use the first 3134 sectors.

Additionally, the 'a' partition must be the first partition on the disk,
immediately following the SGI Volume Header. If the default SGI Volume Header
size is used, the 'a' partition should be located at offset 3135. If the
'a' partition is not located immediately after the SGI Volume Header the
boot loader will not be able to locate and load the kernel.

Do not change any parameters except the partition layout and the label name.

__EOT
	disklabel -c $FSTABFLAG /tmp/fstab.$_disk -E $_disk
}

md_congrats() {
	cat <<__EOT

INSTALL.$ARCH describes how to configure the ARCS PROM to boot OpenBSD.
__EOT
}

md_consoleinfo() {
}
