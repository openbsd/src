#!/bin/ksh
#	$OpenBSD: upgrade.sh,v 1.62 2005/10/12 02:48:49 krw Exp $
#	$NetBSD: upgrade.sh,v 1.2.4.5 1996/08/27 18:15:08 gwr Exp $
#
# Copyright (c) 1997-2004 Todd Miller, Theo de Raadt, Ken Westerback
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

# Have the user confirm that $ROOTDEV is the root filesystem.
while :; do
	ask "Root filesystem?" "$ROOTDEV"
	resp=${resp##*/}
	[[ -b /dev/$resp ]] && break

	echo "Sorry, $resp is not a block device."
done
ROOTDEV=$resp

echo -n "Checking root filesystem (fsck -fp /dev/${ROOTDEV}) ... "
if ! fsck -fp /dev/$ROOTDEV >/dev/null 2>&1; then
	echo	"FAILED.\nYou must fsck ${ROOTDEV} manually."
	exit
fi
echo	"OK."

echo -n "Mounting root filesystem..."
if ! mount -o ro /dev/$ROOTDEV /mnt; then
	echo	"ERROR: can't mount root filesystem!"
	exit
fi
echo	"done."

# The fstab, hosts and myname files are required.
for _file in fstab hosts myname; do
	if [ ! -f /mnt/etc/$_file ]; then
		echo "ERROR: no /mnt/etc/${_file}!"
		exit
	fi
	cp /mnt/etc/$_file /tmp/$_file
done
hostname $(stripcom /tmp/myname)

ask_yn "Enable network using configuration stored on root filesystem?" yes
[[ $resp == y ]] && enable_network

# Offer the user the opportunity to tweak, repair, or create the network
# configuration by hand.
manual_net_cfg

cat <<__EOT

The fstab is configured as follows:

$(</tmp/fstab)

For the $MODE, filesystems in the fstab will be automatically mounted if the
'noauto' option is absent, and /sbin/mount_<fstype> is found, and the fstype is
not nfs. Non-ffs filesystems will be mounted read-only.

You can edit the fstab now, before it is used, but the edited fstab will only
be used during the upgrade. It will not be copied back to disk.
__EOT
edit_tmp_file fstab

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
install_sets

# Perform final steps common to both an install and an upgrade.
finish_up
