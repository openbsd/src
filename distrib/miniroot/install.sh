#!/bin/sh
#	$OpenBSD: install.sh,v 1.143 2004/05/28 22:59:34 krw Exp $
#	$NetBSD: install.sh,v 1.5.2.8 1996/08/27 18:15:05 gwr Exp $
#
# Copyright (c) 1997-2004 Todd Miller, Theo de Raadt, Ken Westerback
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

# The name of the file holding the list of configured filesystems.
FILESYSTEMS=/tmp/filesystems

# The name of the file holding the list of non-default configured swap devices.
SWAPLIST=/tmp/swaplist

# install.sub needs to know the MODE
MODE=install

# include common subroutines and initialization code
. install.sub

# If /etc/fstab already exists, skip disk initialization.
if [ ! -f /etc/fstab ]; then
	# Install the shadowed disktab file; lets us write to it for temporary
	# purposes without mounting the miniroot read-write.
	[ -f /etc/disktab.shadow ] && cp /etc/disktab.shadow /tmp/disktab.shadow

	DISK=
	_DKDEVS=$DKDEVS

	while : ; do
		_DKDEVS=`rmel "$DISK" $_DKDEVS`

		# Always do ROOTDISK first, and repeat until
		# it is configured acceptably.
		if isin $ROOTDISK $_DKDEVS; then
			resp=$ROOTDISK
			rm -f /tmp/fstab
			# Make sure empty files exist so we don't have to
			# keep checking for their existence before grep'ing.
			cat /dev/null >$FILESYSTEMS
			cat /dev/null >$SWAPLIST
		else
			# Force the user to think and type in a disk name by
			# making 'done' the default choice.
			ask_which "disk" "do you wish to initialize" "$_DKDEVS" done "No more disks to initialize"
			[[ $resp == done ]] && break
		fi

		DISK=$resp

		# Deal with disklabels, including editing the root disklabel
		# and labeling additional disks. This is machine-dependent since
		# some platforms may not be able to provide this functionality.
		# /tmp/fstab.$DISK is created here with 'disklabel -f'.
		rm -f /tmp/*.$DISK
		md_prep_disklabel $DISK

		# Get the lists of BSD and swap partitions.
		unset _partitions _psizes _mount_points
		_i=0
		disklabel $DISK 2>&1 | sed -ne '/^ *[a-p]: /p' >/tmp/disklabel.$DISK
		while read _dev _size _offset _type _rest; do
			_pp=${DISK}${_dev%:}
			_ps=$_size

			if [[ $_pp == $ROOTDEV ]]; then
				echo "$ROOTDEV /" >$FILESYSTEMS
				continue
			elif [[ $_type == swap ]]; then
				echo "$_pp" >>$SWAPLIST
				continue
			elif [[ $_type != *BSD ]]; then
				continue
			fi

			_partitions[$_i]=$_pp
			_psizes[$_i]=$_ps

			# If the user assigned a mount point, use it if possible.
			if [[ -f /tmp/fstab.$DISK ]]; then
				while read _pp _mp _rest; do
					[[ $_pp == "/dev/${_partitions[$_i]}" ]] || continue
					# Ignore mount points that have already been specified.
					[[ -n $(grep " $_mp\$" $FILESYSTEMS) ]] && break
					isin $_mp ${_mount_points[*]} && break
					# Ignore '/' for any partition but ROOTDEV. Check just
					# in case ROOTDEV isn't first partition processed.
					[[ $_mp == '/' ]] && break
					# Otherwise, record user specified mount point.
					_mount_points[$_i]=$_mp
				done < /tmp/fstab.$DISK
			fi
			: $(( _i += 1 ))
		done < /tmp/disklabel.$DISK

		if [[ $DISK == $ROOTDISK ]]; then
			# Ensure that ROOTDEV was configured.
			if [[ -n $(grep "^$ROOTDEV /$" $FILESYSTEMS) ]]; then
				echo "The root filesystem will be mounted on $ROOTDEV."
			else
				echo "ERROR: Unable to mount the root filesystem on $ROOTDEV."
				DISK=
			fi
			# Ensure that $SWAPDEV was configured as swap space.
			if [[ -n $(grep "^$SWAPDEV" $SWAPLIST) ]]; then
				echo "$SWAPDEV will be used for swap space."
				# But we really don't want it in the installed
				# /etc/fstab.
				grep -v "^$SWAPDEV" $SWAPLIST > $SWAPLIST.tmp
				mv $SWAPLIST.tmp $SWAPLIST
			else
				echo "ERROR: Unable to use $SWAPDEV for swap space."
				DISK=
			fi
			[[ -n $DISK ]] || echo "You must reconfigure $ROOTDISK."
		fi

		# If there are no BSD partitions, or $DISK has been reset, go on to next disk.
		[[ ${#_partitions[*]} -gt 0 && -n $DISK ]] || continue

		# Now prompt the user for the mount points. Loop until "done" entered.
		_i=0
		while : ; do
			_pp=${_partitions[$_i]}
			_ps=$(( ${_psizes[$_i]} / 2 ))
			_mp=${_mount_points[$_i]}

			# Get the mount point from the user
			ask "Mount point for ${_pp} (size=${_ps}k)? (or 'none' or 'done')" "$_mp"
			case $resp in
			"")	;;
			none)	_mp=
				;;
			done)	break
				;;
			/*)	set -- $(grep " $resp\$" $FILESYSTEMS)
				_pp=$1
				if [[ -z $_pp ]]; then
					# Mount point wasn't specified on a
					# previous disk. Has it been specified
					# on this one?
					_j=0
					for _pp in ${_partitions[*]} ""; do
						if [[ $_i -ne $_j ]]; then
							[[ $resp == ${_mount_points[$_j]} ]] && break
						fi
						: $(( _j += 1 ))
					done
				fi
				if [[ -n $_pp ]]; then
					echo "Invalid response: $_pp is already being mounted at $resp."
					continue
				fi
				_mp=$resp
				;;
			*)	echo "Invalid response: mount point must be an absolute path!"
				continue
				;;
			esac

			_mount_points[$_i]=$_mp

			: $(( _i += 1))
			[ $_i -ge ${#_partitions[*]} ] && _i=0
		done

		# Append mount information to $FILESYSTEMS
		_i=0
		for _pp in ${_partitions[*]}; do
			_mp=${_mount_points[$_i]}
			[ "$_mp" ] && echo "$_pp $_mp" >>$FILESYSTEMS
			: $(( _i += 1 ))
		done
	done

	cat << __EOT

You have configured the following partitions and mount points:

$(<$FILESYSTEMS)

The next step creates a filesystem on each partition, ERASING existing data.
__EOT

	ask_yn "Are you really sure that you're ready to proceed?"
	[[ $resp == n ]] && { echo "ok, try again later..." ; exit ; }

	# Read $FILESYSTEMS, creating a new filesystem on each listed
	# partition and saving the partition and mount point information
	# for subsequent sorting by mount point.
	_i=0
	unset _partitions _mount_points
	while read _pp _mp; do
		_OPT=	
		[[ $_mp == / ]] && _OPT=$MDROOTFSOPT
		newfs -q $_OPT /dev/r$_pp

		_partitions[$_i]=$_pp
		_mount_points[$_i]=$_mp
		: $(( _i += 1 ))
	done <$FILESYSTEMS

	# Write fstab entries to /tmp/fstab in mount point alphabetic
	# order to enforce a rational mount order.
	for _mp in `bsort ${_mount_points[*]}`; do
		_i=0
		for _pp in ${_partitions[*]}; do
			if [ "$_mp" = "${_mount_points[$_i]}" ]; then
				echo -n "/dev/$_pp $_mp ffs rw"
				# Only '/' is neither nodev nor nosuid. i.e.
				# it can obviously *always* contain devices or
				# setuid programs.
				#
				# Every other mounted filesystem is nodev. If
				# the user chooses to mount /dev as a separate
				# filesystem, then on the user's head be it.
				#
				# The only directories that install puts suid
				# binaries into (as of 3.2) are:
				#
				# /sbin
				# /usr/bin
				# /usr/sbin
				# /usr/libexec
				# /usr/libexec/auth
				# /usr/X11R6/bin
				#
				# and ports and users can do who knows what
				# to /usr/local and sub directories thereof.
				#
				# So try to ensure that only filesystems that
				# are mounted at or above these directories
				# can contain suid programs. In the case of
				# /usr/libexec, give blanket permission for
				# subdirectories.
				if [[ $_mp == / ]]; then
					# / can hold devices and suid programs.
					echo " 1 1"
				else
					# No devices anywhere but /.
					echo -n ",nodev"
					case $_mp in
					# A few directories are allowed suid.
					/sbin|/usr)			;;
					/usr/bin|/usr/sbin)		;;
					/usr/libexec|/usr/libexec/*)	;;
					/usr/local|/usr/local/*)	;;
					/usr/X11R6|/usr/X11R6/bin)	;;
					# But all others are not.
					*)	echo -n ",nosuid"	;;
					esac
					echo " 1 2"
				fi
			fi
			: $(( _i += 1 ))
		done
	done >> /tmp/fstab

	# Append all non-default swap devices to fstab.
	while read _dev; do
		echo "/dev/$_dev none swap sw 0 0" >>/tmp/fstab
	done < $SWAPLIST

	munge_fstab
fi

mount_fs "-o async"

# Set hostname.
#
# Use existing hostname (short form) as the default value because we could
# be restarting an install.
#
# Don't ask for, but don't discard, domain information provided by the user.
#
# Only apply the new value if the new short form name differs from the existing
# one. This preserves any existing domain information in the hostname.
ask_until "\nSystem hostname? (short form, e.g. 'foo')" "$(hostname -s)"
[[ ${resp%%.*} != $(hostname -s) ]] && hostname $resp

# Remove existing network configuration files in /tmp to ensure they don't leak
# onto the installed system in the case of a restarted install. Any information
# contained within them should be accessible via ifconfig, hostname, route,
# etc.
( cd /tmp; rm -f host* my* resolv.* dhclient.* )

# Always create new hosts file.
cat > /tmp/hosts << __EOT
::1 localhost
127.0.0.1 localhost
::1 $(hostname -s)
127.0.0.1 $(hostname -s)
__EOT

ask_yn "Configure the network?" yes
[[ $resp == y ]] && donetconfig

_oifs=$IFS
IFS=
resp=
while [[ -z $resp ]]; do
	askpass "Password for root account? (will not echo)"
	_password=$resp

	askpass "Password for root account? (again)"
	if [ "$_password" != "$resp" ]; then
		echo "Passwords do not match, try again."
		resp=
	fi
done
IFS=$_oifs

install_sets

# Remount all filesystems in /etc/fstab with the options from /etc/fstab, i.e.
# without any options such as async which may have been used in the first
# mount.
while read _dev _mp _fstype _opt _rest; do
	mount -u -o $_opt $_dev $_mp ||	exit
done < /etc/fstab

# Handle questions...
questions

echo -n "Saving configuration files..."

# Save any leases obtained during install.
( cd /var/db
[ -f dhclient.leases ] && mv dhclient.leases /mnt/var/db/. )

# Move configuration files from /tmp to /mnt/etc.
( cd /tmp
hostname > myname

# Add FQDN to /tmp/hosts entries, changing lines of the form '1.2.3.4 hostname'
# to '1.2.3.4 hostname.$FQDN hostname'. Leave untouched any lines containing
# domain information or aliases. The user added those manually.
_dn=$(get_fqdn)
while read _addr _hn _aliases; do
	if [[ -n $_aliases || $_hn != ${_hn%%.*} || -z $_dn ]]; then
		echo "$_addr $_hn $_aliases"
	else
		echo "$_addr $_hn.$_dn $_hn"
	fi
done < hosts > hosts.new
mv hosts.new hosts

# Prepend interesting comments from installed hosts and dhclient.conf files
# to /tmp/hosts and /tmp/dhclient.conf.
save_comments hosts
save_comments dhclient.conf

# Possible files: fstab, kbdtype, myname, mygate, sysctl.conf
#                 dhclient.conf resolv.conf resolv.conf.tail
#		  hostname.* hosts
for _f in fstab kbdtype my* *.conf *.tail host*; do
	[[ -f $_f ]] && mv $_f /mnt/etc/.
done )

_encr=`/mnt/usr/bin/encrypt -b 8 -- "$_password"`
echo "1,s@^root::@root:${_encr}:@
w
q" | ed /mnt/etc/master.passwd 2> /dev/null
/mnt/usr/sbin/pwd_mkdb -p -d /mnt/etc /etc/master.passwd

echo -n "done.\nGenerating initial host.random file..."
( cd /mnt/var/db
dd if=/mnt/dev/urandom of=host.random bs=1024 count=64 >/dev/null 2>&1
chmod 600 host.random >/dev/null 2>&1 )
echo "done."

set_timezone

# Perform final steps common to both an install and an upgrade.
finish_up
