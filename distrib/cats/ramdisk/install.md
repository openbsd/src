#	$OpenBSD: install.md,v 1.2 2004/02/02 21:01:19 drahn Exp $
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

MDFSTYPE=msdos
MDXAPERTURE=2
ARCH=ARCH

md_set_term() {
}

md_installboot() {
	local _disk=$1

	[[ $disklabeltype == MBR ]] || return

	echo -n "Copying 'ofwboot' to the boot partition (${_disk}i)..."
	if mount -t msdos /dev/${_disk}i /mnt2 ; then
		if cp /usr/mdec/ofwboot /mnt2; then
			umount /mnt2
			echo "done."
			return
		fi
	fi

	echo "FAILED.\nYou will not be able to boot OpenBSD from $_disk."
	exit
}

md_prep_disk() {
	local _disk=$1 _resp
	typeset -l _resp

	cat << __EOT

$_disk must be partitioned using an MBR partition table.

MBR partition tables are created with OpenBSD. MacOS *cannot* be booted from a
disk partitioned with an MBR partition table.

__EOT

	md_prep_MBR $_disk
}


md_prep_disklabel() {
	local _disk=$1 _q

	md_prep_disk $_disk

	case $disklabeltype in
	MBR)	cat << __EOT

You *MUST* setup the OpenBSD disklabel to include the MSDOS-formatted boot
partition as the 'i' partition. If the 'i' partition is missing or not the
MSDOS-formatted boot partition, then the 'ofwboot' file required to boot
OpenBSD cannot be installed.

__EOT
		;;
	*)	echo "Disk label type ('$disklabeltype') is not 'MBR'."
		exit
		;;
	esac

	disklabel -W $_disk >/dev/null 2>&1
	disklabel -c -f /tmp/fstab.$_disk -E $_disk
}

md_congrats() {
	cat << __EOT

Once the machine has rebooted use OpenFirmware to boot into OpenBSD, as
described in the INSTALL.$ARCH document. The command to boot OpenBSD will be
something like 'boot (hd0)bsd'.

__EOT
}
