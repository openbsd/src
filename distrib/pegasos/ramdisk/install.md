#	$OpenBSD: install.md,v 1.1 2003/10/31 03:59:05 drahn Exp $
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

	echo -n "Copying 'ofwboot' to the boot partition"
	if cp /usr/mdec/ofwboot /mnt; then
		echo "done."
		return
	fi

	echo "FAILED.\nYou will not be able to boot OpenBSD from $_disk."
	exit
}

md_prep_disk() {
	local _disk=$1 _resp
	typeset -l _resp

	local _disk=$1

	if [[ -n $(disklabel -c $_disk 2>/dev/null | grep ' HFS ') ]]; then
		cat << __EOT

WARNING: putting an MBR partition table on $_disk will DESTROY the existing HFS
         partitions and HFS partition table.

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

		return
	fi
 
	# Manual MBR setup. The user is basically on their own. Give a few
	# hints and let the user rip.
	cat << __EOT

**** NOTE ****

A valid MBR for an OpenBSD bootable disk must contain at least:

a) One OpenBSD (id 'A6') partition.

**************

Current partition information is:

$(fdisk $_disk)

__EOT

	fdisk -e $_disk

	cat << __EOT
Here is the MBR configuration you chose:

$(fdisk $_disk)

Please take note of the offsets and sizes of any DOS partitions, the OpenBSD
partition, and any other partitions you want to access from OpenBSD. You may
need this information to fill in the OpenBSD disklabel.

__EOT
}


md_prep_disklabel() {
	local _disk=$1 _q

	md_prep_disk $_disk

	disklabel -W $_disk >/dev/null 2>&1
	disklabel -c -f /tmp/fstab.$_disk -E $_disk
}

md_congrats() {

	cat << __EOT

Once the machine has rebooted use OpenFirmware to boot into OpenBSD, as
described in the INSTALL.$ARCH document. The command to boot OpenBSD will be
something like 'boot hd:3,ofwboot /bsd'.

__EOT
}

# Ask the user for a y or n, and insist on 'y', 'yes', 'n' or 'no'.
#
#    $1    = the question to ask the user
#    $2    = the default answer (assumed to be 'n' if empty).
#
# Return 'y' or 'n' in $resp.
ask_yn () {
	local _q=$1 _a=${2:-no} _resp
	typeset -l _resp

	while : ; do
		ask "$_q" "$_a"
		_resp=$resp
		case $_resp in
		y|yes)	resp=y ; return ;;
		n|no)	resp=n ; return ;;
		esac
	done		
 }
