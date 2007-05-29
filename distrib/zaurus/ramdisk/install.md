#	$OpenBSD: install.md,v 1.10 2007/05/29 21:13:56 robert Exp $
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

ARCH=ARCH

md_installboot() {
}

# $1 is the disk to check
md_checkfordisklabel() {
	local rval=0

	disklabel -r $1 >/dev/null 2>/tmp/checkfordisklabel

	if grep "disk label corrupted" /tmp/checkfordisklabel; then
		rval=2
	fi >/dev/null 2>&1

	rm -f /tmp/checkfordisklabel
	return $rval
}

md_prep_fdisk() {
	local _disk=$1

	ask_yn "Do you want to use *all* of $_disk for OpenBSD?"
	if [[ $resp == y ]]; then
		echo -n "Putting all of $_disk into an active OpenBSD MBR partition (type 'A6')..."
		fdisk -e ${_disk} <<__EOT >/dev/null
reinit
update
write
quit
__EOT
		echo "done."
		return
	fi

	# Manually configure the MBR.
	cat <<__EOT

You will now create a single MBR partition to contain your OpenBSD data. This
partition must have an id of 'A6'; must *NOT* overlap other partitions; and
must be marked as the only active partition.

The 'manual' command describes all the fdisk commands in detail.

$(fdisk ${_disk})
__EOT
	fdisk -e ${_disk}

	cat <<__EOT
Here is the partition information you chose:

$(fdisk ${_disk})
__EOT
}

md_prep_disklabel() {
	local _disk=$1

	md_prep_fdisk $_disk

	cat <<__EOT

You will now create an OpenBSD disklabel inside the OpenBSD MBR
partition. The disklabel defines how OpenBSD splits up the MBR partition
into OpenBSD partitions in which filesystems and swap space are created.

The offsets used in the disklabel are ABSOLUTE, i.e. relative to the
start of the disk, NOT the start of the OpenBSD MBR partition.

__EOT

	md_checkfordisklabel $_disk
	case $? in
	2)	echo "WARNING: Label on disk $_disk is corrupted. You will be repairing it.\n"
		;;
	esac

	disklabel -W $_disk >/dev/null 2>&1
	disklabel -f /tmp/fstab.$_disk -E $_disk
}

md_congrats() {
	val=`ztsscale`
	case $? in
	0)
		grep -v '^mouse\.scale.*$' /mnt/etc/wsconsctl.conf \
		     >/tmp/sysctl.conf
		echo $val "	# see ztsscale(8)" >> /tmp/wsconsctl.conf
		cp /tmp/wsconsctl.conf /mnt/etc/wsconsctl.conf
		;;
	esac
}
