#!/bin/sh
#	$OpenBSD: install.sh,v 1.16 1997/10/17 12:21:02 deraadt Exp $
#	$NetBSD: install.sh,v 1.5.2.8 1996/08/27 18:15:05 gwr Exp $
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
FQDN=""					# domain name

trap "umount /tmp > /dev/null 2>&1" 0

MODE="install"

# include machine-dependent functions
# The following functions must be provided:
#	md_copy_kernel()	- copy a kernel to the installed disk
#	md_get_diskdevs()	- return available disk devices
#	md_get_cddevs()		- return available CD-ROM devices
#	md_get_ifdevs()		- return available network interfaces
#	md_get_partition_range() - return range of valid partition letters
#	md_installboot()	- install boot-blocks on disk
#	md_labeldisk()		- put label on a disk
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
	md_welcome_banner
	echo -n "Proceed with installation? [n] "
else
	echo "You seem to be trying to restart an interrupted installation!"
	echo ""
	echo "You can try to skip the disk preparation steps and continue,"
	echo "otherwise you should reboot the miniroot and start over..."
	echo -n "Skip disk initialization? [n] "
fi
getresp "n"
case "$resp" in
	y*|Y*)
		echo	""
		echo	"Cool!  Let's get to it..."
		;;
	*)
		md_not_going_to_install
		exit
		;;
esac

# XXX Work around vnode aliasing bug (thanks for the tip, Chris...)
ls -l /dev > /dev/null 2>&1

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

	while [ "X${ROOTDISK}" = "X" ]; do
		getrootdisk
	done

	# Deal with disklabels, including editing the root disklabel
	# and labeling additional disks.  This is machine-dependent since
	# some platforms may not be able to provide this functionality.
	md_prep_disklabel ${ROOTDISK}

	# Assume partition 'a' of $ROOTDISK is for the root filesystem.
	# Loop and get the rest.
	# XXX ASSUMES THAT THE USER DOESN'T PROVIDE BOGUS INPUT.
	cat << \__get_filesystems_1

You will now have the opportunity to enter filesystem information.  You will be
prompted for device name and mount point (full path, including the prepending
'/' character).  (NOTE: these do not have to be in any particular order).
__get_filesystems_1

	echo	"The following will be used for the root filesystem:"
	echo	"	${ROOTDISK}a	/"

	echo	"${ROOTDISK}a /" > ${FILESYSTEMS}

	resp="X"	# force at least one iteration
	while [ "X$resp" != X"done" ]; do
		echo	""
		echo -n	"Device name? [done] "
		getresp "done"
		case "$resp" in
		done)
			;;

		*)
			_device_name=`basename $resp`

			# force at least one iteration
			_first_char="X"
			while [ "X${_first_char}" != X"/" ]; do
				echo -n "Mount point? "
				getresp ""
				_mount_point=$resp
				_first_char=`firstchar ${_mount_point}`
				if [ "X${_first_char}" != X"/" ]; then
					echo "mount point must be an absolute path!"
				fi
			done
			if [ "X${_mount_point}" = X"/" ]; then
				echo "root mount point already taken care of!"
			else
				echo "${_device_name} ${_mount_point}" \
					>> ${FILESYSTEMS}
			fi
			resp="X"	# force loop to repeat
			;;
		esac
	done

	echo	""
	echo	"You have configured the following devices and mount points:"
	echo	""
	cat ${FILESYSTEMS}
	echo	""
	echo	"Filesystems will now be created on these devices."
	echo 	"If you made any mistakes, you may edit this now."
	echo -n	"Edit using ${EDITOR}? [n] "
	getresp "n"
	case "$resp" in
		y*|Y*)
			${EDITOR} ${FILESYSTEMS}
			;;
		*)
			;;
	esac
	echo
	echo "============================================================"
	echo "The next step will overwrite any existing data on:"
	(
		echo -n "	"
		while read _device_name _junk; do
			echo -n "${_device_name} "
		done
		echo ""
	) < ${FILESYSTEMS}
	echo	""

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
			newfs /dev/r${_device_name}
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
		resp=""		# force at least one iteration
		_nam=""
		if [ -f /tmp/myname ]; then
			_nam=`cat /tmp/myname`
		fi
		while [ "X${resp}" = X"" ]; do
			echo -n "Enter system hostname (short form): [$_nam] "
			getresp "$_nam"
		done
		hostname $resp
		echo $resp > /tmp/myname

		resp=""		# force at least one iteration
		if [ -f /tmp/resolv.conf ]; then
			FQDN=`grep '^domain ' /tmp/resolv.conf | \
			    sed -e 's/^domain //'`
		fi
		while [ "X${resp}" = X"" ]; do
			echo -n "Enter DNS domain name: [$FQDN] "
			getresp "$FQDN"
		done
		FQDN=$resp

		configurenetwork

		resp="none"
		if [ -f /tmp/mygate ]; then
			resp=`cat /tmp/mygate`
		fi
		echo -n "Enter IP address of default route: [$resp] "
		getresp "$resp"
		if [ "X${resp}" != X"none" ]; then
			route delete default > /dev/null 2>&1
			if route add default $resp > /dev/null ; then
				echo $resp > /tmp/mygate
			fi
		fi

		resp="none"
		if [ -f /tmp/resolv.conf ]; then
			resp=`grep '^nameserver ' /tmp/resolv.conf | \
			    sed -e 's/^nameserver //'`
		fi
		echo -n	"Enter IP address of primary nameserver: [$resp] "
		getresp "$resp"
		if [ "X${resp}" != X"none" ]; then
			echo "domain $FQDN" > /tmp/resolv.conf
			echo "nameserver $resp" >> /tmp/resolv.conf
			echo "search $FQDN" >> /tmp/resolv.conf

			echo -n "Would you like to use the nameserver now? [y] "
			getresp "y"
			case "$resp" in
				y*|Y*)
					cp /tmp/resolv.conf \
					    /tmp/resolv.conf.shadow
					;;

				*)
					;;
			esac
		fi

		if [ ! -f /tmp/resolv.conf.shadow ]; then 
			echo ""
			echo "The host table is as follows:"
			echo ""
			cat /tmp/hosts
			echo ""
			echo "You may want to edit the host table in the event that"
			echo "you are doing an NFS installation or an FTP installation"
			echo "without a name server and want to refer to the server by"
			echo "name rather than by its numeric ip address."
			echo -n "Would you like to edit the host table with ${EDITOR}? [n] "
			getresp "n"
			case "$resp" in
				y*|Y*)
					${EDITOR} /tmp/hosts
					;;
	
				*)
					;;
			esac
		fi

		cat << \__network_config_2

You will now be given the opportunity to escape to the command shell to do
any additional network configuration you may need.  This may include adding
additional routes, if needed.  In addition, you might take this opportunity
to redo the default route in the event that it failed above.
__network_config_2
		echo -n "Escape to shell? [n] "
		getresp "n"
		case "$resp" in
			y*|Y*)
				echo "Type 'exit' to return to install."
				sh
				;;

			*)
				;;
		esac
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
#	echo	""
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
#	echo ""

	munge_fstab /tmp/fstab /tmp/fstab.shadow
	mount_fs /tmp/fstab.shadow
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

install_sets $THESETS

# Copy in configuration information and make devices in target root.

if [ ! -d /mnt/etc -o ! -d /mnt/usr/share/zoneinfo -o ! -d /mnt/dev ]; then
	echo "Something needed to complete the installation seems"
	echo "to be missing, did you forget to extract a required set?"
	echo ""
	echo "Please review the installation notes and try again..."
	echo ""
	echo "You *may* be able to correct the problem and type 'install'"
	echo "without having to extract all of the distribution sets again."
	exit
fi

cd /tmp
for file in fstab hostname.* hosts myname mygate resolv.conf; do
	if [ -f $file ]; then
		echo -n "Copying $file..."
		cp $file /mnt/etc/$file
		echo "done."
	fi
done

# If no zoneinfo on the installfs, give them a second chance
if [ ! -e /usr/share/zoneinfo ]; then
	get_timezone
fi
if [ ! -e /mnt/usr/share/zoneinfo ]; then
	echo "Cannot install timezone link..."
else
	echo -n "Installing timezone link..."
	rm -f /mnt/etc/localtime
	ln -s /usr/share/zoneinfo/$TZ /mnt/etc/localtime
	echo "done."
fi


md_copy_kernel

md_installboot ${ROOTDISK}

if [ ! -x /mnt/dev/MAKEDEV ]; then
	echo "No /dev/MAKEDEV installed, something is wrong here..."
	exit
fi

echo -n "Making all devices..."
#pid=`twiddle`
cd /mnt/dev
sh MAKEDEV all
#kill $pid
echo "done."
cd /

unmount_fs /tmp/fstab.shadow

# Pat on the back.
md_congrats

# ALL DONE!
exit 0
