#!/bin/sh
#	$NetBSD: upgrade.sh,v 1.1 1996/01/06 22:45:15 pk Exp $
#
# Copyright (c) 1995 Jason R. Thorpe.
# All rights reserved.
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
#	This product includes software developed for the NetBSD Project
#	by Jason R. Thorpe.
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#	NetBSD installation script.
#	In a perfect world, this would be a nice C program, with a reasonable
#	user interface.

VERSION=1.1
export VERSION				# XXX needed in subshell
ROOTDISK=""				# filled in below

trap "umount /tmp > /dev/null 2>&1" 0

MODE="upgrade"

# include machine-dependent functions
# The following functions must be provided:
#	md_get_diskdevs()	- return available disk devices
#	md_get_cddevs()		- return available CD-ROM devices
#	md_get_ifdevs()		- return available network interfaces
#	md_installboot()	- install boot-blocks on disk
#	md_checkfordisklabel()	- check for valid disklabel
#	md_labeldisk()		- put label on a disk
#	md_welcome_banner()	- display friendly message
#	md_not_going_to_install() - display friendly message
#	md_congrats()		- display friendly message
. install.md

# include common subroutines
. install.sub

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

# We don't like it, but it sure makes a few things a lot easier.
do_mfs_mount "/tmp" "2048"

while [ "X${ROOTDISK}" = "X" ]; do
	getrootdisk
done

# Make sure there's a disklabel there.  If there isn't, puke after
# disklabel prints the error message.
md_checkfordisklabel ${ROOTDISK}
case $rval in
	1)
		cat << \__disklabel_not_present_1

FATAL ERROR: There is no disklabel present on the root disk!  You must
label the disk with SYS_INST before continuing.

__disklabel_not_present_1
		exit
		;;

	2)
		cat << \__disklabel_corrupted_1

FATAL ERROR: The disklabel on the root disk is corrupted!  You must
re-label the disk with SYS_INST before continuing.

__disklabel_corrupted_1
		exit
		;;

	*)
		;;
esac

# Assume partition 'a' of $ROOTDISK is for the root filesystem.  Confirm
# this with the user.  Check and mount the root filesystem.
resp=""			# force one iteration
while [ "X${resp}" = "X" ]; do
	echo -n	"Root filesystem? [${ROOTDISK}a] "
	getresp "${ROOTDISK}a"
	_root_filesystem="/dev/`basename $resp`"
	if [ ! -b ${_root_filesystem} ]; then
		echo "Sorry, ${resp} is not a block device."
		resp=""	# force loop to repeat
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

# Now that the network has been configured, it is safe to configure the
# fstab.  We remove all but ufs/ffs/nfs.
(
	rm -f /tmp/fstab.new
	while read line; do
		_fstype=`echo $line | awk '{print $3}'`
		if [ "X${_fstype}" = X"ufs" -o \
		    "X${_fstype}" = X"ffs" -o \
		    "X${_fstype}" = X"nfs" ]; then
			echo $line >> /tmp/fstab.new
		fi
	done
) < /tmp/fstab

if [ ! -f /tmp/fstab.new ]; then
	echo	"ERROR: strange fstab!"
	exit 1
fi

# Convert ufs to ffs.
sed -e 's/ufs/ffs/' < /tmp/fstab.new > /tmp/fstab
rm -f /tmp/fstab.new

echo	"The fstab is configured as follows:"
echo	""
cat /tmp/fstab
cat << \__fstab_config_1

You may wish to edit the fstab.  For example, you may need to resolve
dependencies in the order which the filesystems are mounted.  Note that
this fstab is only for installation purposes, and will not be copied into
the root filesystem.

__fstab_config_1
echo -n	"Edit the fstab? [n] "
getresp "n"
case "$resp" in
	y*|Y*)
		vi /tmp/fstab
		;;

	*)
		;;
esac

echo	""
munge_fstab /tmp/fstab /tmp/fstab.shadow

if ! umount /mnt; then
	echo	"ERROR: can't unmount previously mounted root!"
	exit 1
fi

# Check all of the filesystems.
check_fs /tmp/fstab.shadow

# Mount filesystems.
mount_fs /tmp/fstab.shadow

# Install sets.
install_sets $UPGRSETS

# Get timezone info
get_timezone

# Fix up the fstab.
echo -n	"Converting ufs to ffs in /etc/fstab..."
sed -e 's/ufs/ffs/' < /mnt/etc/fstab > /tmp/fstab
echo	"done."
echo -n	"Would you like to edit the resulting fstab? [y] "
getresp "y"
case "$resp" in
	y*|Y*)
		vi /tmp/fstab
		;;

	*)
		;;
esac

# Copy in configuration information and make devices in target root.
(
	cd /tmp
	for file in fstab; do
		if [ -f $file ]; then
			echo -n "Copying $file..."
			cp $file /mnt/etc/$file
			echo "done."
		fi
	done

	echo -n "Installing timezone link..."
	rm -f /mnt/etc/localtime
	ln -s /usr/share/zoneinfo/$TZ /mnt/etc/localtime
	echo "done."

	echo -n "Making devices..."
	pid=`twiddle`
	cd /mnt/dev
	sh MAKEDEV all
	kill $pid
	echo "done."

	echo -n "Copying kernel..."
	cp -p /netbsd /mnt/netbsd
	echo "done."

	md_installboot ${ROOTDISK}
)

unmount_fs /tmp/fstab.shadow

# Pat on the back.
md_congrats

# ALL DONE!
exit 0
