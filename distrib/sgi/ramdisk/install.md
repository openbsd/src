#	$OpenBSD: install.md,v 1.2 2004/08/26 13:32:15 pefo Exp $
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
MDTERM=vt220
ARCH=ARCH

md_set_term() {
}

md_installboot() {
# Nothing to do. Boot is installed when preparing volume header.
}

md_checkfordisklabel() {
	# $1 is the disk to check
	local rval

	disklabel $1 > /dev/null 2> /tmp/checkfordisklabel
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

md_prep_disklabel()
{
	local _disk

	_disk=$1
	echo
	echo "Checking SGI Volume Header:"
	/usr/mdec/sgivol -q $_disk > /dev/null 2> /dev/null
	case $? in
	0)	/usr/mdec/sgivol $_disk
	cat << __EOT

A SGI Volume Header was found on the disk. Normally you want to replace it
with a new Volume Header suitable for installing OpenBSD. Doing this will
of course delete all data currently on the disk.
__EOT
		ask "Do you want to overwrite the current header?" y
		case "$resp" in
		y*|Y*)
			/usr/mdec/sgivol -qi $_disk
			;;
		n*|N*)
			cat << __EOT

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
					exit 1
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
			exit 1 ;;
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
			exit 1
			;;
		esac
		;;
	esac

	echo "Installing boot loader in volume header."
	/usr/mdec/sgivol -wf boot /usr/mdec/boot $_disk
	case $? in
	0)
		;;
	*)	echo
		echo "WARNING: Boot install failed. Booting from disk will not be possible"
		;;
	esac

	cat << __EOT

You will now create an OpenBSD disklabel. The default disklabel have an 'a'
partition which is the space available for OpenBSD. The 'i' partition must
be retained since it contains the Volume Header nad the boot.

Do not change any parameters except the partition layout and the label name.
Also, don't change the 'i' partition or start any other partition below the
end of the 'i' partition. This is the Volume Header and destroying it will
render the disk useless.

__EOT
	md_checkfordisklabel $_disk
	case $? in
	2)	echo "WARNING: Label on disk $_disk is corrupted. You will be repairing it.\n"
		;;
	esac

	disklabel -W $_disk
	disklabel -c -f /tmp/fstab.$_disk -E $_disk
}

md_congrats() {
	cat << __EOT

Your machine is now set up to boot OpenBSD. Normally the ARCS PROM will
set up the system to boot from the first disk found with a valid Volume
Header. If the 'OSLoader' environment variable already is set to 'boot'
nothing in the firmware setup should need to be changed. To set up booting
refer to the install document.

To reboot the system, just enter 'reboot' at the shell prompt.
__EOT
}
