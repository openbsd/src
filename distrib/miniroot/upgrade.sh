#!/bin/sh
#	$OpenBSD: upgrade.sh,v 1.19 2001/11/18 22:48:58 krw Exp $
#	$NetBSD: upgrade.sh,v 1.2.4.5 1996/08/27 18:15:08 gwr Exp $
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

#	OpenBSD installation script.
#	In a perfect world, this would be a nice C program, with a reasonable
#	user interface.

trap "unmount_fs -check /tmp/fstab.shadow > /dev/null 2>&1; rm -f /tmp/fstab.shadow" 0

MODE="upgrade"

# include machine-dependent functions
# The following functions must be provided:
#	md_get_diskdevs()	- return available disk devices
#	md_get_cddevs()		- return available CD-ROM devices
#	md_get_partition_range() - return range of valid partition letters
#	md_installboot()	- install boot-blocks on disk
#	md_labeldisk()		- put label on a disk
#	md_welcome_banner()	- display friendly message
#	md_not_going_to_install() - display friendly message
#	md_congrats()		- display friendly message

# include machine dependent subroutines
. install.md

# include common subroutines
. install.sub

# which sets?
THESETS="$UPGRSETS $MDSETS"

# Good {morning,afternoon,evening,night}.
md_welcome_banner
echo -n "Proceed with upgrade? [n] "
getresp "n"
case "$resp" in
	y*|Y*)
		echo	"Cool!  Let's get to it..."
		;;
	*)
		md_not_going_to_install
		exit
		;;
esac

# Deal with terminal issues
md_set_term

# XXX Work around vnode aliasing bug (thanks for the tip, Chris...)
ls -l /dev > /dev/null 2>&1

while [ "X${ROOTDISK}" = "X" ]; do
	getrootdisk
done

# Assume partition 'a' of $ROOTDISK is for the root filesystem.  Confirm
# this with the user.  Check and mount the root filesystem.
resp=
while [ "X${resp}" = "X" ]; do
	echo -n	"Root filesystem? [${ROOTDISK}a] "
	getresp "${ROOTDISK}a"
	_root_filesystem="/dev/`basename $resp`"
	if [ ! -b ${_root_filesystem} ]; then
		echo "Sorry, ${resp} is not a block device."
		resp=
	fi
done

echo	"Checking root filesystem..."
if ! fsck -pf ${_root_filesystem}; then
	echo	"ERROR: can't check root filesystem!"
	exit 1
fi

echo	"Mounting root filesystem..."
if ! mount -o ro ${_root_filesystem} /mnt; then
	echo	"ERROR: can't mount root filesystem!"
	exit 1
fi

# Grab the fstab so we can munge it for our own use.
if [ ! -f /mnt/etc/fstab ]; then
	echo	"ERROR: no /etc/fstab!"
	exit 1
fi
cp /mnt/etc/fstab /tmp/fstab

# Grab the hosts table so we can use it.
if [ ! -f /mnt/etc/hosts ]; then
	echo	"ERROR: no /etc/hosts!"
	exit 1
fi
cp /mnt/etc/hosts /tmp/hosts

# Start up the network in same/similar configuration as the installed system
# uses.
cat << \__network_config_1

The upgrade program would now like to enable the network.  It will use the
configuration already stored on the root filesystem.  This is required
if you wish to use the network installation capabilities of this program.

__network_config_1
echo -n	"Enable network? [y] "
getresp "y"
case "$resp" in
	y*|Y*)
		if ! enable_network; then
			echo "ERROR: can't enable network!"
			exit 1
		fi

		cat << \__network_config_2

You will now be given the opportunity to escape to the command shell to
do any additional network configuration you may need.  This may include
adding additional routes, if needed.  In addition, you might take this
opportunity to redo the default route in the event that it failed above.

__network_config_2
		echo -n "Escape to shell? [n] "
		getresp "n"
		case "$resp" in
			y*|Y*)
				echo "Type 'exit' to return to upgrade."
				sh
				;;

			*)
				;;
		esac
		;;
	*)
		;;
esac

echo	"The fstab is configured as follows:\n"
cat /tmp/fstab

cat << \__fstab_config_1

You may wish to edit the fstab.  For example, you may need to resolve
dependencies in the order which the filesystems are mounted.

NOTE: 1) this fstab is used only during the upgrade. It will not be
         copied into the root filesystem.

      2) all non-ffs filesystems, and filesystems with the 'noauto'
         option, will be ignored during the upgrade.

__fstab_config_1
echo -n	"Edit the fstab with ${EDITOR}? [n] "
getresp "n"
case "$resp" in
	y*|Y*)
		${EDITOR} /tmp/fstab
		;;

	*)
		;;
esac

echo	""

# Create a fstab containing only ffs filesystems w/o 'noauto'.
munge_fstab /tmp/fstab /tmp/fstab.shadow

if ! umount /mnt; then
	echo	"ERROR: can't unmount previously mounted root!"
	exit 1
fi

# Check filesystems.
check_fs /tmp/fstab.shadow

# Mount filesystems.
mount_fs /tmp/fstab.shadow

# If Xfree86 v3 directories that would prevent upgrading to XFree86 v4
# are found, move them and replace them with links that the upgrade
# can replace with new values.
(
if [ -d /mnt/usr/X11R6/lib/X11 ]; then
	cd /mnt/usr/X11R6/lib/X11
	for xf3dir in twm xkb xsm xinit rstart; do
		if [ -e $xf3dir -a ! -L $xf3dir ]; then
			mkdir -p XF3
			mv $xf3dir XF3/.
			ln -s XF3/$xf3dir $xf3dir
		fi
	done
fi
)

echo -n	"Are the upgrade sets on one of your normally mounted (local) filesystems? [y] "
getresp "y"
case "$resp" in
	y*|Y*)
		get_localdir /mnt
		;;
	*)
		;;
esac

# Install sets.
install_sets $THESETS

# Get timezone info
get_timezone

# Copy in configuration information and make devices in target root.
(
	echo "Installing timezone link."
	rm -f /mnt/etc/localtime
	ln -s /usr/share/zoneinfo/$TZ /mnt/etc/localtime

	if [ -f /mnt/etc/sendmail.cf -a ! -f /mnt/etc/mail/sendmail.cf ]; then
		echo "Moving /etc/sendmail.cf -> /etc/mail/sendmail.cf"
		test -d /mnt/etc/mail || mkdir /mnt/etc/mail
		mv /mnt/etc/sendmail.cf /mnt/etc/mail/sendmail.cf
		ed - /mnt/etc/rc << \__rc_edit
1,$s/etc\/sendmail.cf/etc\/mail\/sendmail.cf/g
w
q
__rc_edit
	fi

	echo -n "Making devices..."
	cd /mnt/dev
	sh MAKEDEV all
	echo "done."

	md_installboot ${ROOTDISK}
)
populateusrlocal
test -x /mnt/upgrade.site && /mnt/usr/sbin/chroot /mnt /upgrade.site

unmount_fs /tmp/fstab.shadow

# Pat on the back.
md_congrats

# ALL DONE!
exit 0
