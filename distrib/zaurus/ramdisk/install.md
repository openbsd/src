#	$OpenBSD: install.md,v 1.1 2004/12/31 00:14:07 drahn Exp $
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
}

md_prep_disk() {
	local _disk=$1 _resp
	typeset -l _resp

	cat << __EOT

$_disk must be partitioned using an BSD or an MBR partition table.

BSD partition table or MBR partition tables can be created by openbsd.
It is more a question of firmware compatiblity disk portability.
(Once we can figure out what filesystems ABLE can boot)
__EOT

	while : ; do
		ask "Use BSD or MBR partition table?" BSD
		_resp=$resp
		case $_resp in
		m|mbr)	export disklabeltype=MBR
			md_prep_MBR $_disk
			break
			;;
		b|bsd)	export disklabeltype=BSD
			md_prep_BSD $_disk
			break
			;;
		esac
	done
}

md_prep_MBR() {
	local _disk=$1

	if [[ -n $(disklabel -c $_disk 2>/dev/null | grep ' BSD ') ]]; then
		cat << __EOT

WARNING: putting an MBR partition table on $_disk will DESTROY the existing BSD
         partitions and BSD partition table.

__EOT
		ask_yn "Are you *sure* you want an MBR partition table on $_disk?"
		[[ $resp == n ]] && exit
	fi

	ask_yn "Use *all* of $_disk for OpenBSD?"
	if [[ $resp == y ]]; then
		echo -n "Creating Master Boot Record (MBR)..."
		fdisk -e $_disk  >/dev/null 2>&1 << __EOT
reinit
update
write
quit
__EOT
		echo "done."

		echo -n "Formatting 1MB MSDOS boot partition..."
		gunzip < /usr/mdec/msdos1mb.gz | \
		    dd of=/dev/r${_disk}c bs=512 seek=1 >/dev/null 2>&1
		echo "done."

		return
	fi

	# Manual MBR setup. The user is basically on their own. Give a few
	# hints and let the user rip.
	cat << __EOT

**** NOTE ****

XXX

**************

Current partition information is:

$(fdisk $_disk)

__EOT

	fdisk -e $_disk

	cat << __EOT
Here is the MBR configuration you chose:

$(fdisk $_disk)

Please take note of the offsets and sizes of the DOS partition, the OpenBSD
partition, and any other partitions you want to access from OpenBSD. You will
need this information to fill in the OpenBSD disklabel.

__EOT
}

md_prep_BSD() {
	local _disk=$1

	cat << __EOT

No special setup should be required to label using BSD disklabel.
however if the disk has previously been partitioned in another
manner, it may be necessary to wipe existing partition tables
before proceeding.

dd if=/dev/zero of=/${_disk} bs=512 count=10
disklabel -c ${_disk}


__EOT

}

md_prep_disklabel() {
	local _disk=$1 _q

	md_prep_disk $_disk

	case $disklabeltype in
	BSD)	;;
	MBR)	cat << __EOT

You *MUST* setup the OpenBSD disklabel to include the MSDOS-formatted boot
partition as the 'i' partition. If the 'i' partition is missing or not the
MSDOS-formatted boot partition, then the kernel required to boot
OpenBSD cannot be installed.

__EOT
		;;
	*)	echo "Disk label type ('$disklabeltype') is not 'BSD' or 'MBR'."
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
