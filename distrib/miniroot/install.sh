#!/bin/sh
#	$OpenBSD: install.sh,v 1.70 2000/03/19 01:14:03 espie Exp $
#	$NetBSD: install.sh,v 1.5.2.8 1996/08/27 18:15:05 gwr Exp $
#
# Copyright (c) 1997,1998 Todd Miller, Theo de Raadt
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

FILESYSTEMS="/tmp/filesystems"		# used thoughout
FQDN=					# domain name

trap "umount /tmp > /dev/null 2>&1" 0

MODE="install"

# include machine-dependent functions
# The following functions must be provided:
#	md_get_diskdevs()	- return available disk devices
#	md_get_cddevs()		- return available CD-ROM devices
#	md_get_partition_range() - return range of valid partition letters
#	md_installboot()	- install boot-blocks on disk
#	md_prep_disklabel()	- label the root disk
#	md_welcome_banner()	- display friendly message
#	md_not_going_to_install() - display friendly message
#	md_congrats()		- display friendly message
#	md_native_fstype()	- native filesystem type for disk installs
#	md_native_fsopts()	- native filesystem options for disk installs
#	md_makerootwritable()	- make root writable (at least /tmp)
#	md_machine_arch()	- get machine architecture

# include machine dependent subroutines
. install.md

# include common subroutines
. install.sub

# which sets?
THESETS="$ALLSETS $MDSETS"

if [ "`df /`" = "`df /mnt`" ]; then
	# Good {morning,afternoon,evening,night}.
	echo ==================================================
	md_welcome_banner
else
	echo "You seem to be trying to restart an interrupted installation!"
	echo
	echo "You can try to skip the disk preparation steps and continue,"
	echo "otherwise you should reboot the miniroot and start over..."
	echo -n "Skip disk initialization? [n] "
	getresp "n"
	case "$resp" in
		y*|Y*)
			echo
			echo "Cool!  Let's get to it..."
			echo
			;;
		*)
			md_not_going_to_install
			exit
			;;
	esac
fi


echo "You can run a shell command at any prompt via '!foo'"
echo "or escape to a shell by simply typing '!'."
echo

# Deal with terminal issues
md_set_term

# Get timezone info
get_timezone

# Make sure we can write files (at least in /tmp)
# This might make an MFS mount on /tmp, or it may
# just re-mount the root with read-write enabled.
if [ "`df /`" = "`df /tmp`" ]; then
	md_makerootwritable
fi

# Get the machine architecture (must be done after md_makerootwritable)
ARCH=`md_machine_arch`

if [ "`df /`" = "`df /mnt`" ]; then
	# Install the shadowed disktab file; lets us write to it for temporary
	# purposes without mounting the miniroot read-write.
	if [ -f /etc/disktab.shadow ]; then
		cp /etc/disktab.shadow /tmp/disktab.shadow
	fi

	while : ; do
		if [ "X${ROOTDISK}" = "X" ]; then
			while [ "X${ROOTDISK}" = "X" ]; do
				getrootdisk
			done
			DISK=$ROOTDISK
		else
			DISK=
			while [ "X${DISK}" = "X" ]; do
				getanotherdisk
			done
			if [ "${DISK}" = "done" ]; then
				break
			fi
		fi

		# Deal with disklabels, including editing the root disklabel
		# and labeling additional disks.  This is machine-dependent since
		# some platforms may not be able to provide this functionality.
		md_prep_disklabel ${DISK}

		# Assume partition 'a' of $ROOTDISK is for the root filesystem.
		# Loop and get the rest.
		# XXX ASSUMES THAT THE USER DOESN'T PROVIDE BOGUS INPUT.
		cat << __get_filesystems_1

You will now have the opportunity to enter filesystem information for ${DISK}.
You will be prompted for the mount point (full path, including the prepending
'/' character) for each BSD partition on ${DISK}.  Enter "none" to skip a
partition or "done" when you are finished.
__get_filesystems_1

		if [ "${DISK}" = "${ROOTDISK}" ]; then
			echo
			echo	"The following partitions will be used for the root filesystem and swap:"
			echo	"	${ROOTDISK}a	/"
			echo	"	${ROOTDISK}b	swap"

			echo	"${ROOTDISK}a /" > ${FILESYSTEMS}
		fi

		# XXX - allow the user to name mount points on disks other than ROOTDISK
		#	also allow a way to enter non-BSD partitions (but don't newfs!)
		# Get the list of BSD partitions and store sizes
		_npartitions=0
		for _p in `disklabel ${DISK} 2>&1 | grep '^ *[a-p]:.*BSD' | sed 's/^ *\([a-p]\): *\([0-9][0-9]*\) .*/\1\2/'`; do
			_pp=`firstchar ${_p}`
			if [ "${DISK}" = "${ROOTDISK}" -a "$_pp" = "a" ]; then
				continue
			fi
			_ps=`echo ${_p} | sed 's/^.//'`
			_partitions[${_npartitions}]=${_pp}
			_psizes[${_npartitions}]=${_ps}
			# If the user assigned a mount point, use it.
			if [ -f /tmp/fstab.${DISK} ]; then
				_mount_points[${_npartitions}]=`sed -n "s:^/dev/$DISK$_pp[ 	]*\([^ 	]*\).*:\1:p" < /tmp/fstab.${DISK}`
			fi
			_npartitions=$(( ${_npartitions} + 1 ))
		done

		# Now prompt the user for the mount points.  Loop until "done"
		echo
		_i=0
		resp="X"
		while [ $_npartitions -gt 0 -a X${resp} != X"done" ]; do
			_pp=${_partitions[${_i}]}
			_ps=$(( ${_psizes[${_i}]} / 2 ))
			_mp=${_mount_points[${_i}]}

			# Get the mount point from the user
			while : ; do
				echo -n "Mount point for ${DISK}${_pp} (size=${_ps}k) [$_mp, RET, none, or done]? "
				getresp "$_mp"
				case "X${resp}" in
					X/*)	_mount_points[${_i}]=$resp
						break ;;
					Xdone|X)
						break ;;
					Xnone)	_mount_points[${_i}]=
						break;;
					*)	echo "mount point must be an absolute path!";;
				esac
			done
			_i=$(( ${_i} + 1 ))
			if [ $_i -ge $_npartitions ]; then
				_i=0
			fi
		done

		# Now write it out
		_i=0
		while test $_i -lt $_npartitions; do
			if [ -n "${_mount_points[${_i}]}" ]; then
				echo "${DISK}${_partitions[${_i}]} ${_mount_points[${_i}]}" >> ${FILESYSTEMS}
				_mount_points[${_i}]=
			fi
			_i=$(( ${_i} + 1 ))
		done
		rm -f /tmp/fstab.${DISK}
	done

	echo
	echo	"You have configured the following devices and mount points:"
	echo
	cat ${FILESYSTEMS}
	echo
	echo "============================================================"
	echo "The next step will overwrite any existing data on:"
	(
		echo -n "	"
		while read _device_name _junk; do
			echo -n "${_device_name} "
		done
		echo
	) < ${FILESYSTEMS}
	echo

	echo -n	"Are you really sure that you're ready to proceed? [n] "
	getresp "n"
	case "$resp" in
		y*|Y*)
			;;
		*)
			echo "ok, try again later..."
			exit
			;;
	esac

	# Loop though the file, place filesystems on each device.
	echo	"Creating filesystems..."
	(
		while read _device_name _junk; do
			newfs -q /dev/r${_device_name}
		done
	) < ${FILESYSTEMS}
else
	# Get the root device
	ROOTDISK=`df /mnt | sed -e '/^\//!d' -e 's/\/dev\/\([^ ]*\)[a-p] .*/\1/'`
	while [ "X${ROOTDISK}" = "X" ]; do
		getrootdisk
	done
fi

# Get network configuration information, and store it for placement in the
# root filesystem later.
cat << \__network_config_1

You will now be given the opportunity to configure the network.  This will be
useful if you need to transfer the installation sets via FTP, HTTP, or NFS.
Even if you choose not to transfer installation sets that way, this information
will be preserved and copied into the new root filesystem.

__network_config_1
echo -n	"Configure the network? [y] "
getresp "y"
case "$resp" in
	y*|Y*)
		donetconfig
		;;
	*)
		;;
esac

if [ "`df /`" = "`df /mnt`" ]; then
	# Now that the network has been configured, it is safe to configure the
	# fstab.
	(
		while read _dev _mp; do
			if [ "$_mp" = "/" ]; then
				echo /dev/$_dev $_mp ffs rw 1 1
			else
				echo /dev/$_dev $_mp ffs rw 1 2
			fi
		done
	) < ${FILESYSTEMS} > /tmp/fstab

# XXX We no longer do the following. It is not neccessary. It can be done
# XXX after the install is complete.
#
#	echo	"The fstab is configured as follows:"
#	echo
#	cat /tmp/fstab
#	cat << \__fstab_config_1
#
#You may wish to edit the fstab.  You may also wish to take this opportunity to
#place NFS mounts in the fstab  (this would be especially useful if you plan to
#keep '/usr' on an NFS server.
#__fstab_config_1
#	echo -n	"Edit the fstab with ${EDITOR}? [n] "
#	getresp "n"
#	case "$resp" in
#		y*|Y*)
#			${EDITOR} /tmp/fstab
#			;;
#
#		*)
#			;;
#	esac
#
#	echo

	munge_fstab /tmp/fstab /tmp/fstab.shadow
	mount_fs /tmp/fstab.shadow "-o async"
fi

mount | while read line; do
	set -- $line
	if [ "$2" = "/" -a "$3" = "nfs" ]; then
		echo "You appear to be running diskless."
		echo -n	"Are the install sets on one of your currently mounted filesystems? [n] "
		getresp "n"
		case "$resp" in
			y*|Y*)
				get_localdir
				;;
			*)
				;;
		esac
	fi
done

resp=		# force one iteration
echo
echo 'Please enter the initial password that the root account will have.'
while [ "X${resp}" = X"" ]; do
	echo -n "Password (will not echo): "
	stty -echo
	getresp "${_password}"
	stty echo
	echo
	_password=$resp

	echo -n "Password (again): "
	stty -echo
	getresp ""
	stty echo
	echo
	if [ "${_password}" != "${resp}" ]; then
		echo "Passwords do not match, try again."
		resp=
	fi
done

md_questions

install_sets $THESETS

# XXX
# XXX should loop until successful install or user abort
# XXX
if [ X"$ssl" != X1 ]; then
	resp=
	while [ X"${resp}" = X ]; do
		echo
		echo "Two OpenBSD libraries (libssl and libcrypto, based on OpenSSL) implement many"
		echo "cryptographic functions which are used by OpenBSD programs like ssh, httpd, and"
		echo "isakmpd.  Due to patent licensing reasons, those libraries may not be included"
		echo "on the CD -- instead the base distribution contains libraries which have had"
		echo "the troublesome code removed -- the programs listed above will not be fully"
		echo "functional as a result.  Libraries which _include_ the troublesome routines"
		echo "are available and can be FTP installed, as long as you meet the follow (legal)"
		echo "criteria:"
		echo "  (1) Outside the USA, no restrictions apply. Use ssl${VERSION}.tar.gz."
		echo "  (2) Inside the USA, non-commercial entities may install sslUSA${VERSION}.tar.gz."
		echo "  (3) Commercial entities in the USA are left in the cold, due to how the"
		echo "      licences work.  (This is how the USA crypto export policy feels to the"
		echo "      rest of the world.)"
		echo ""
		echo "If you do not install the ssl package now, it is easily installed at"
		echo "a later time (see the afterboot(8) and ssl(8) manual pages)."
		echo -n "Install (U)SA, (I)nternational, or (N)one? [none] "

		getresp none
		case "$resp" in
		u*|U*)
			THESETS=sslUSA
			;;
		i*|I*)
			THESETS=ssl
			;;
		n*|N*)
			echo "Not installing SSL+RSA shared libraries."
			THESETS=
			;;
		*)
			echo "Invalid response: $resp"
			resp=
			;;
		esac
	done
	if [ X"$THESETS" != X ]; then
		resp=
		while [ X"${resp}" = X ]; do
			echo -n "Install SSL+RSA libraries via (f)tp, (h)ttp, or (c)ancel? [ftp] "
			getresp ftp
			case "$resp" in
			f*|F*)
				# configure network if necessary
				test -n "$_didnet" || donetconfig

				install_url -ftp -reuse -minpat ${THESETS}'[0-9]*'
				resp=f
				;;
			h*|H*)
				# configure network if necessary
				test -n "$_didnet" || donetconfig

				install_url -http -reuse -minpat ${THESETS}'[0-9]*'
				resp=h
				;;
			c*|C*)
				echo "Not installing SSL+RSA shared libraries."
				;;
			*)
				echo "Invalid response: $resp"
				resp=
				;;
			esac
		done
	fi
	echo
fi

# Copy in configuration information and make devices in target root.
echo
cd /tmp
echo -n "Copying "
for file in fstab hostname.* hosts myname mygate resolv.conf; do
	if [ -f $file ]; then
		echo -n "$file, "
		cp $file /mnt/etc/$file
		rm -f $file
	fi
done
echo " ...done."

if [ -f /etc/dhclient.conf ]; then
	echo -n "Modifying dhclient.conf..."
	cat /etc/dhclient.conf >> /mnt/etc/dhclient.conf
fi

# If no zoneinfo on the installfs, give them a second chance
if [ ! -e /usr/share/zoneinfo ]; then
	get_timezone
fi
if [ ! -e /mnt/usr/share/zoneinfo ]; then
	echo "Cannot install timezone link."
else
	echo "Installing timezone link."
	rm -f /mnt/etc/localtime
	ln -s /usr/share/zoneinfo/$TZ /mnt/etc/localtime
fi


if [ ! -x /mnt/dev/MAKEDEV ]; then
	echo "No /dev/MAKEDEV installed, something is wrong here..."
	exit
fi

echo -n "Making all device nodes (by running /dev/MAKEDEV all) ..."
cd /mnt/dev
sh MAKEDEV all
echo "... done."
cd /

remount_fs /tmp/fstab.shadow
md_installboot ${ROOTDISK}

_encr=`echo ${_password} | /mnt/usr/bin/encrypt -b 7`
echo "1,s@^root::@root:${_encr}:@
w
q" | ed /mnt/etc/master.passwd 2> /dev/null
/mnt/usr/sbin/pwd_mkdb -p -d /mnt/etc /etc/master.passwd

dd if=/mnt/dev/urandom of=/mnt/var/db/host.random bs=1024 count=64 >/dev/null 2>&1
chmod 600 /mnt/var/db/host.random >/dev/null 2>&1
populateusrlocal

unmount_fs /tmp/fstab.shadow

# Pat on the back.
md_congrats

# ALL DONE!
exit 0
