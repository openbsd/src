#!/bin/sh
#	$OpenBSD: upgrade.sh,v 1.43 2002/10/03 00:56:44 krw Exp $
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

# Have the user confirm that $ROOTDEV is the root filesystem.
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

# The fstab, hosts and myname files are required.
for _file in fstab hosts myname; do
	if [ ! -f /mnt/etc/$_file ]; then
		echo "ERROR: no /etc/${_file}!"
		exit
	fi
	cp /mnt/etc/$_file /tmp/$_file
done

# Set the FQDN and system hostname (short form).
HOSTNAME=`cat /tmp/myname`
FQDN=$HOSTNAME
HOSTNAME=${HOSTNAME%%.*}
FQDN=${FQDN#${HOSTNAME}}
FQDN=${FQDN#.}
[[ -n $FQDN ]] || get_resolv_fqdn /mnt/etc/resolv.conf
hostname $HOSTNAME.$FQDN

# Start up the network in same/similar configuration as the installed system
# uses.
ask "Enable network using configuration stored on root filesystem?" y
case $resp in
y*|Y*)
	if ! enable_network; then
		echo "ERROR: can't enable network!"
		exit
	fi

	ask "Do you want to do more, manual, network configuration?" n
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

You can edit the fstab now, before it is used, but the edited fstab will
only be used during the upgrade. It will not be copied back to disk.

Filesystems in the fstab will be mounted only if the 'noauto' option is
absent, /sbin/mount_<fstype> is found, and the fstype is not nfs. Only
filesystems with a fstype of ffs will be mounted read-write.

__EOT
ask "Edit the fstab with ${EDITOR}?" n
case $resp in
y*|Y*)	${EDITOR} /tmp/fstab
	;;
esac

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
