#!/bin/sh
#	$OpenBSD: install.sh,v 1.122 2002/11/28 01:54:58 krw Exp $
#	$NetBSD: install.sh,v 1.5.2.8 1996/08/27 18:15:05 gwr Exp $
#
# Copyright (c) 1997-2002 Todd Miller, Theo de Raadt, Ken Westerback
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
#	This product includes software developed by Todd Miller and
#	Theo de Raadt
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
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

# A list of devices holding filesystems and the associated mount points
# is kept in the file named FILESYSTEMS.
FILESYSTEMS=/tmp/filesystems

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
			rm -f $FILESYSTEMS
		else
			ask_which "disk" "do you wish to initialize?" "$_DKDEVS"
			[ "$resp" = "done" ] && break
		fi

		DISK=$resp

		# Deal with disklabels, including editing the root disklabel
		# and labeling additional disks. This is machine-dependent since
		# some platforms may not be able to provide this functionality.
		# /tmp/fstab.$DISK is created here with 'disklabel -f'.
		rm -f /tmp/fstab.$DISK
		md_prep_disklabel $DISK

		# Get the list of BSD partitions and store sizes
		# XXX - It would be nice to just pipe the output of sed to a
		#       'while read _pp _ps' loop, but our 'sh' runs the last
		#       element of a pipeline in a subshell and the required side
		#       effects to _partitions, etc. would be lost.
		unset _partitions _psizes _mount_points
		_i=0
		for _p in $(disklabel ${DISK} 2>&1 | sed -ne '/^ *\([a-p]\): *\([0-9][0-9]*\).*BSD.*/s//\1\2/p'); do
			# All characters after the initial [a-p] are the partition size
			_ps=${_p#?}
			# Removing the partition size leaves us with the partition name
			_pp=${DISK}${_p%${_ps}}

			if [[ $_pp == $ROOTDEV ]]; then
				echo "$ROOTDEV /" > $FILESYSTEMS
				continue
			fi

			_partitions[$_i]=$_pp
			_psizes[$_i]=$_ps

			# If the user assigned a mount point, use it if possible.
			if [[ -f /tmp/fstab.$DISK ]]; then
				while read _pp _mp _rest; do
					[[ $_pp == "/dev/${_partitions[$_i]}" ]] || continue
					# Ignore mount points that have already been specified.
					[[ -f $FILESYSTEMS && -n $(grep " $_mp\$" $FILESYSTEMS) ]] && break
					isin $_mp ${_mount_points[*]} && break
					# Ignore '/' for any partition but ROOTDEV. Check just
					# in case ROOTDEV isn't first partition processed. 
					[[ $_mp == '/' ]] && break					
					# Otherwise, record user specified mount point.
					_mount_points[$_i]=$_mp
				done < /tmp/fstab.$DISK
			fi
			: $(( _i += 1 ))
 		done

		if [[ $DISK == $ROOTDISK ]]; then
			# Ensure that ROOTDEV was configured.
			if [[ -f $FILESYSTEMS && -n $(grep "^$ROOTDEV /$" $FILESYSTEMS) ]]; then
				echo "The root filesystem will be mounted on $ROOTDEV."
			else
				echo "ERROR: Unable to mount the root filesystem on $ROOTDEV."
				DISK=
			fi
			# Ensure that ${ROOTDISK}b was configured as swap space.
			if [[ -n $(disklabel $ROOTDISK 2>&1 | sed -ne '/^ *\(b\):.*swap/s//\1/p') ]]; then
				echo "${ROOTDISK}b will be used for swap space."
			else
				echo "ERROR: Unable to use ${ROOTDISK}b for swap space."
				DISK=
			fi
			[[ -n $DISK ]] || echo "You must reconfigure $ROOTDISK."
		fi

		# If there are no BSD partitions, or $DISK has been reset, go on to next disk.
		[[ ${#_partitions[*]} > 0 && -n $DISK ]] || continue
		
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
			/*)	_pp=`grep " $resp\$" $FILESYSTEMS | cutword 1`
				if [ -z "$_pp" ]; then
					# Mount point wasn't specified on a previous disk. Has it
					# been specified on this one?
					_j=0
					for _pp in ${_partitions[*]} ""; do
						if [ $_i -ne $_j ]; then	
							[ "$resp" = "${_mount_points[$_j]}" ] && break
						fi	
						: $(( _j += 1 ))
					done
				fi
				if [ "$_pp" ]; then
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
			[ "$_mp" ] && echo "$_pp $_mp" >> $FILESYSTEMS
			: $(( _i += 1 ))
		done
	done

	cat << __EOT

You have configured the following partitions and mount points:

$(<$FILESYSTEMS)

The next step creates a filesystem on each partition, ERASING existing data.
__EOT

	ask "Are you really sure that you're ready to proceed?" n
	case $resp in
	y*|Y*)	;;
	*)	echo "ok, try again later..."
		exit
		;;
	esac

	# Read $FILESYSTEMS, creating a new filesystem on each listed
	# partition and saving the partition and mount point information
	# for subsequent sorting by mount point.
	_i=0
	unset _partitions _mount_points
	while read _pp _mp; do
		newfs -q /dev/r$_pp

		_partitions[$_i]=$_pp
		_mount_points[$_i]=$_mp
		: $(( _i += 1 ))
	done < $FILESYSTEMS

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

	munge_fstab
fi

mount_fs "-o async"

ask_until "\nEnter system hostname (short form, e.g. 'foo'):"
HOSTNAME=$resp
FQDN=my.domain
hostname $HOSTNAME.$FQDN

# Get network configuration information, and store it for placement in the
# root filesystem later.
ask "Configure the network?" y
case $resp in
y*|Y*)	donetconfig
	;;
*)	cat > /tmp/hosts << __EOT
::1 localhost.$FQDN localhost
127.0.0.1 localhost.$FQDN localhost
::1 $HOSTNAME.$FQDN $HOSTNAME
127.0.0.1 $HOSTNAME.$FQDN $HOSTNAME
__EOT
	;;
esac

_oifs=$IFS
IFS=
resp=
while [ -z "$resp" ]; do
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

# Set machdep.apertureallowed if required. install_sets must be
# done first so that /etc/sysctl.conf is available.
set_machdep_apertureallowed
	
# Copy configuration files to /mnt/etc.
cfgfiles="fstab hostname.* mygate resolv.conf kbdtype sysctl.conf"

echo -n "Saving configuration files..."
if [ -f /etc/dhclient.conf ]; then
	cat /etc/dhclient.conf >> /mnt/etc/dhclient.conf
	echo "lookup file bind" > /mnt/etc/resolv.conf.tail
	cp /var/db/dhclient.leases /mnt/var/db/.
	# Don't install mygate for dhcp installations.
	# Note that mygate should not be the first or last file
	# in cfgfiles or this won't work.
	cfgfiles=`echo $cfgfiles | sed -e 's/ mygate / /'`
fi

hostname > /mnt/etc/myname

cd /tmp

# Try to retain useful leading comments in /etc/hosts file.
grep "^#" /mnt/etc/hosts > hosts.comment
cat hosts.comment hosts > /mnt/etc/hosts

for file in $cfgfiles; do
	if [ -f $file ]; then
		cp $file /mnt/etc/$file
		rm -f $file
	fi
done
echo "...done."

remount_fs

_encr=`/mnt/usr/bin/encrypt -b 8 -- "$_password"`
echo "1,s@^root::@root:${_encr}:@
w
q" | ed /mnt/etc/master.passwd 2> /dev/null
/mnt/usr/sbin/pwd_mkdb -p -d /mnt/etc /etc/master.passwd

echo -n "Generating initial host.random file ..."
dd if=/mnt/dev/urandom of=/mnt/var/db/host.random bs=1024 count=64 >/dev/null 2>&1
chmod 600 /mnt/var/db/host.random >/dev/null 2>&1
echo "...done."

# Perform final steps common to both an install and an upgrade.
finish_up
