#!/bin/ksh
#	$OpenBSD: install.sh,v 1.185 2009/05/06 08:35:24 deraadt Exp $
#	$NetBSD: install.sh,v 1.5.2.8 1996/08/27 18:15:05 gwr Exp $
#
# Copyright (c) 1997-2009 Todd Miller, Theo de Raadt, Ken Westerback
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

# The name of the file holding the list of configured filesystems.
FILESYSTEMS=/tmp/filesystems

# install.sub needs to know the MODE
MODE=install

# include common subroutines and initialization code
. install.sub

# If /etc/fstab already exists, skip disk initialization.
if [[ ! -f /etc/fstab ]]; then
	DISK=
	_DKDEVS=$DKDEVS

	while :; do
		_DKDEVS=$(rmel "$DISK" $_DKDEVS)

		# Always do ROOTDISK first, and repeat until it is configured.
		if isin $ROOTDISK $_DKDEVS; then
			resp=$ROOTDISK
			rm -f /tmp/fstab
			# Make sure empty files exist so we don't have to
			# keep checking for their existence before grep'ing.
			cat /dev/null >$FILESYSTEMS
		else
			# Force the user to think and type in a disk name by
			# making 'done' the default choice.
			ask_which "disk" "do you wish to initialize" "$_DKDEVS" done
			[[ $resp == done ]] && break
		fi

		DISK=$resp
		makedev $DISK || continue

		# Deal with disklabels, including editing the root disklabel
		# and labeling additional disks. This is machine-dependent since
		# some platforms may not be able to provide this functionality.
		# /tmp/fstab.$DISK is created here with 'disklabel -f'.
		rm -f /tmp/*.$DISK
		AUTOROOT=n
		md_prep_disklabel $DISK

		# Get the lists of BSD and swap partitions.
		unset _partitions _mount_points
		_i=0
		disklabel $DISK 2>&1 | sed -ne '/^ *[a-p]: /p' >/tmp/disklabel.$DISK
		while read _dev _size _offset _type _rest; do
			_pp=$DISK${_dev%:}

			if [[ $_pp == $ROOTDEV ]]; then
				echo "$ROOTDEV /" >$FILESYSTEMS
				continue
			elif [[ $_pp == $SWAPDEV ]]; then
				continue
			elif [[ $_type == swap ]]; then
				echo "/dev/$_pp none swap sw 0 0" >>/tmp/fstab
				continue
			elif [[ $_type != *BSD ]]; then
				continue
			fi

			_partitions[$_i]=$_pp

			# Set _mount_points[$_i].
			while read _pp _mp _rest; do
				[[ $_pp == /dev/${_partitions[$_i]} ]] && \
				       { _mount_points[$_i]=$_mp ; break ; }
			done </tmp/fstab.$DISK
			: $(( _i += 1 ))
		done </tmp/disklabel.$DISK

		if [[ $DISK == $ROOTDISK && -z $(grep "^$ROOTDEV /$" $FILESYSTEMS) ]]; then
			echo "ERROR: No root partition ($ROOTDEV)."
			DISK=
			continue
		fi

		# If there are no BSD partitions go on to next disk.
		(( ${#_partitions[*]} > 0 )) || continue

		# Ignore mount points that have already been specified.
		_i=0
		while (( _i < ${#_mount_points[*]} )); do
			_mp=${_mount_points[$_i]}
			grep -q " $_mp$" $FILESYSTEMS && continue

			# Append mount information to $FILESYSTEMS
			_pp=${_partitions[$_i]}
			echo "$_pp $_mp" >>$FILESYSTEMS

			: $(( _i += 1))
		done
	done

	# Read $FILESYSTEMS, creating a new filesystem on each listed
	# partition and saving the partition and mount point information
	# for subsequent sorting by mount point.
	_i=0
	unset _partitions _mount_points
	while read _pp _mp; do
		_OPT=
		[[ $_mp == / ]] && _OPT=$MDROOTFSOPT
		newfs -q $_OPT /dev/r$_pp
		# N.B.: '!' is lexically < '/'. That is required for correct
		#	sorting of mount points.
		_mount_points[$_i]="$_mp!$_pp"
		: $(( _i += 1 ))
	done <$FILESYSTEMS

	# Write fstab entries to /tmp/fstab in mount point alphabetic order
	# to enforce a rational mount order.
	for _mp in $(bsort ${_mount_points[*]}); do
		_pp=${_mp##*!}
		_mp=${_mp%!*}
		echo -n "/dev/$_pp $_mp ffs rw"

		# Only '/' is neither nodev nor nosuid. i.e. it can obviously
		# *always* contain devices or setuid programs.
		[[ $_mp == / ]] && { echo " 1 1" ; continue ; }

		# Every other mounted filesystem is nodev. If the user chooses
		# to mount /dev as a separate filesystem, then on the user's
		# head be it.
		echo -n ",nodev"

		# The only directories that the install puts suid binaries into
		# (as of 3.2) are:
		#
		# /sbin
		# /usr/bin
		# /usr/sbin
		# /usr/libexec
		# /usr/libexec/auth
		# /usr/X11R6/bin
		#
		# and ports and users can do who knows what to /usr/local and
		# sub directories thereof.
		#
		# So try to ensure that only filesystems that are mounted at
		# or above these directories can contain suid programs. In the
		# case of /usr/libexec, give blanket permission for
		# subdirectories.
		case $_mp in
		/sbin|/usr)			;;
		/usr/bin|/usr/sbin)		;;
		/usr/libexec|/usr/libexec/*)	;;
		/usr/local|/usr/local/*)	;;
		/usr/X11R6|/usr/X11R6/bin)	;;
		*)	echo -n ",nosuid"	;;
		esac
		echo " 1 2"
	done >>/tmp/fstab

	munge_fstab
fi

mount_fs "-o async"

[[ $MODE == install ]] && set_timezone /var/tzdir/

install_sets

[[ $MODE == install ]] && set_timezone /mnt/usr/share/zoneinfo/

# Remount all filesystems in /etc/fstab with the options from /etc/fstab, i.e.
# without any options such as async which may have been used in the first
# mount.
while read _dev _mp _fstype _opt _rest; do
	mount -u -o $_opt $_dev $_mp ||	exit
done </etc/fstab

# Ensure an enabled console has the correct speed in /etc/ttys.
sed -e "/^console.*on.*secure.*$/s/std\.[0-9]*/std.$(stty speed)/" \
	/mnt/etc/ttys >/tmp/ttys
# Move ttys back in case questions() needs to massage it more.
mv /tmp/ttys /mnt/etc/ttys

echo -n "Saving configuration files..."

# Save any leases obtained during install.
( cd /var/db
[ -f dhclient.leases ] && mv dhclient.leases /mnt/var/db/. )

# Move configuration files from /tmp to /mnt/etc.
( cd /tmp
hostname >myname

# Append entries to installed hosts file, changing '1.2.3.4 hostname'
# to '1.2.3.4 hostname.$FQDN hostname'. Leave untouched lines containing
# domain information or aliases. These are lines the user added/changed
# manually. Note we may have no hosts file if no interfaces were configured.
if [[ -f hosts ]]; then
	_dn=$(get_fqdn)
	while read _addr _hn _aliases; do
		if [[ -n $_aliases || $_hn != ${_hn%%.*} || -z $_dn ]]; then
			echo "$_addr\t$_hn $_aliases"
		else
			echo "$_addr\t$_hn.$_dn $_hn"
		fi
	done <hosts >>/mnt/etc/hosts
	rm hosts
fi

# Append dhclient.conf to installed dhclient.conf.
_f=dhclient.conf
[[ -f $_f ]] && { cat $_f >>/mnt/etc/$_f ; rm $_f ; }

# Possible files: fstab hostname.* kbdtype mygate myname ttys
#		  boot.conf resolv.conf sysctl.conf resolv.conf.tail
# Save only non-empty (-s) regular (-f) files.
for _f in fstab hostname* kbdtype my* ttys *.conf *.tail; do
	[[ -f $_f && -s $_f ]] && mv $_f /mnt/etc/.
done )

echo -n "done.\nGenerating initial host.random file..."
( cd /mnt/var/db
/mnt/bin/dd if=/mnt/dev/urandom of=host.random bs=1024 count=64 >/dev/null 2>&1
chmod 600 host.random >/dev/null 2>&1 )
echo "done."

apply

if [[ -n $user ]]; then
	_encr="*"
	[[ -n "$userpass" ]] && _encr=`/mnt/usr/bin/encrypt -b 8 -- "$userpass"`
	userline="${user}:${_encr}:1000:10::0:0:${username}:/home/${user}:/bin/ksh"
	echo "$userline" >> /mnt/etc/master.passwd

	mkdir -p /mnt/home/$user
	(cd /mnt/etc/skel; cp -pR . /mnt/home/$user)
	cp -p /mnt/var/mail/root /mnt/var/mail/$user
	chown -R 1000.10 /mnt/home/$user /mnt/var/mail/$user
	echo "1,s@wheel:.:0:root\$@wheel:\*:0:root,${user}@
w
q" | /mnt/bin/ed /mnt/etc/group 2>/dev/null
fi

if [[ -n "$_rootpass" ]]; then
	_encr=`/mnt/usr/bin/encrypt -b 8 -- "$_rootpass"`
	echo "1,s@^root::@root:${_encr}:@
w
q" | /mnt/bin/ed /mnt/etc/master.passwd 2>/dev/null
fi
/mnt/usr/sbin/pwd_mkdb -p -d /mnt/etc /etc/master.passwd

# Perform final steps common to both an install and an upgrade.
finish_up
