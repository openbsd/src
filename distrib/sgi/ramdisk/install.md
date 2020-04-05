#	$OpenBSD: install.md,v 1.45 2020/04/05 15:15:42 krw Exp $
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
MDKERNEL=GENERIC-$IPARCH

# FFS2 not supported, use FFS1
MDFSOPT=-O1
MDROOTFSOPT=-O1

MDSETS="bsd.$IPARCH bsd.rd.$IPARCH"
MDSANESETS=bsd.$IPARCH
if ((NCPU > 1)); then
	MDSETS="$MDSETS bsd.mp.$IPARCH"
	MDSANESETS="$MDSANESETS bsd.mp.$IPARCH"
fi

md_installboot() {
	local _disk=$1

	echo "Installing boot loader in volume header."
	if ! /usr/mdec/sgivol -w boot /mnt/usr/mdec/boot-$IPARCH $_disk; then
		echo "\nWARNING: Boot install failed. Booting from disk will not be possible"
	fi

	for _k in /mnt/bsd{,.mp,.rd}; do
		[[ -f $_k.$IPARCH ]] && mv $_k.$IPARCH $_k
	done 
}

md_prep_disklabel()
{
	local _disk=$1 _f=/tmp/i/fstab.$1

	echo "\nChecking SGI Volume Header:"
	/usr/mdec/sgivol -q $_disk >/dev/null 2>/dev/null
	case $? in
	0)	/usr/mdec/sgivol $_disk
	cat <<__EOT

An SGI Volume Header was found on the disk. Normally you want to replace it
with a new Volume Header suitable for installing OpenBSD. Doing this will
of course delete all data currently on the disk.
__EOT
		if ask_yn "Do you want to overwrite the current header?" y; then
			/usr/mdec/sgivol -qi $_disk
		else
			cat <<__EOT

If the Volume Header was installed by a previous OpenBSD install keeping
it is OK as long as the Volume Header has room for the 'boot' program.
If you are trying to keep an old IRIX Volume Header, OpenBSD install will
use the 'a' partition on the disk for the install and any data in that
partition will be lost.
__EOT
			if ! ask_yn "Are you sure you want to try to keep the old header?" y; then
				ask_yn "Do you want to overwrite the old header instead?" ||
					return 1
				/usr/mdec/sgivol -qi $_disk
			fi
		fi
		;;
	1)	echo "\nYour disk seems to be unaccessible. It was not possible"
		echo "determine if there is a proper Volume Header or not."
		ask_yn "Do you want to continue anyway?" || return 1
		;;
	2)	echo "\nThere is no Volume Header found on the disk. A Volume"
		echo "header is required to be able to boot from the disk."
		ask_yn "Do you want to install a Volume Header?" y || return 1
		/usr/mdec/sgivol -qi $_disk
		;;
	esac

	disklabel_autolayout $_disk $_f || return
	[[ -s $_f ]] && return

	cat <<__EOT

You will now create an OpenBSD disklabel. The disklabel must have an
'a' partition, being the space available for OpenBSD's root file system.
The 'p' partition must be retained since it contains the SGI Volume Header;
this in turn contains the boot loader. No other partitions should overlap
with the SGI Volume Header, which by default will use the first 3134 sectors.

Do not change any parameters except the partition layout and the label name.

__EOT
	disklabel -c -F /tmp/i/fstab.$_disk -E $_disk
}

md_congrats() {
	cat <<__EOT

INSTALL.$ARCH describes how to configure the ARCS PROM to boot OpenBSD.
__EOT
}

md_consoleinfo() {
}
