#!/bin/sh
#	$OpenBSD: upgrade.sh,v 1.40 2002/07/27 04:05:09 krw Exp $
#	$NetBSD: upgrade.sh,v 1.2.4.5 1996/08/27 18:15:08 gwr Exp $
#
# Copyright (c) 1997-2002 Todd Miller, Theo de Raadt, Ken Westerback
# All rights reserved.
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

# install.sub needs to know the MODE
MODE=upgrade

# include common subroutines and initialization code
. install.sub

# Remove 'etc' set from THESETS. It should be installed
# manually, after the upgrade. Note that etc should not
# be the first or last set in THESETS, or this won't
# work!
THESETS=`echo $THESETS | sed -e 's/ etc / /'`

# XXX Work around vnode aliasing bug (thanks for the tip, Chris...)
ls -l /dev > /dev/null 2>&1

# Get ROOTDISK and default ROOTDEV 
get_rootdisk

# Assume $ROOTDEV is the root filesystem. Confirm
# this with the user. Check and mount the root filesystem.
resp=
while [ -z "$resp" ]; do
	ask "Root filesystem?" "$ROOTDEV"
	resp=${resp##*/}
	if [ ! -b /dev/$resp ]; then
		echo "Sorry, ${resp} is not a block device."
		resp=
	fi
done
ROOTDEV=$resp

echo -n "Checking root filesystem (fsck -fp /dev/${ROOTDEV}) ... "
if ! fsck -fp /dev/$ROOTDEV > /dev/null 2>&1; then
	echo	"FAILED.\nYou must fsck ${ROOTDEV} manually."
	exit
fi
echo	"OK."

echo -n "Mounting root filesystem ... "
if ! mount -o ro /dev/$ROOTDEV /mnt; then
	echo	"ERROR: can't mount root filesystem!"
	exit
fi
echo	"Done."

# fstab and hosts are required for upgrade
for _file in fstab hosts; do
	if [ ! -f /mnt/etc/$_file ]; then
		echo "ERROR: no /etc/${_file}!"
		exit
	fi
	cp /mnt/etc/$_file /tmp/$_file
done

# Start up the network in same/similar configuration as the installed system
# uses.
cat << __EOT

The upgrade program would now like to enable the network. It will use the
configuration already stored on the root filesystem. This is required
if you wish to use the network installation capabilities of this program.

__EOT
ask "Enable network?" y
case $resp in
y*|Y*)
	if ! enable_network; then
		echo "ERROR: can't enable network!"
		exit
	fi

	cat << __EOT

You will now be given the opportunity to escape to the command shell to
do any additional network configuration you may need. This may include
adding additional routes, if needed. In addition, you might take this
opportunity to redo the default route in the event that it failed above.

__EOT
	ask "Escape to shell?" n
	case $resp in
	y*|Y*)	echo "Type 'exit' to return to upgrade."
		sh
		;;
	esac
	;;
esac

cat << __EOT
The fstab is configured as follows:

$(</tmp/fstab)

You may wish to edit the fstab before the filesystems are mounted. e.g. to
change the order in which the filesystems are mounted.

NOTE:	1) the edited fstab will be used only during the upgrade. It will not
           be copied back into the root filesystem.

	2) A filesystem will not be mounted if
		a) the 'noauto' option is present,
		b) /sbin/mount_<fstype> is not found,
		c) the fstype is nfs.

	3) Non-ffs filesystems will be mounted read-only.

__EOT
ask "Edit the fstab with ${EDITOR}?" n
case $resp in
y*|Y*)	${EDITOR} /tmp/fstab
	;;
esac

echo

# Create /etc/fstab.
munge_fstab

# fsck -p non-root filesystems in /etc/fstab.
check_fs

# Mount filesystems in /etc/fstab.
if ! umount /mnt; then
	echo	"ERROR: can't unmount previously mounted root!"
	exit
fi
mount_fs

# Install sets.
install_sets $THESETS

# Perform final steps common to both an install and an upgrade.
finish_up
