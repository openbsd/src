#!/bin/sh
#	$OpenBSD: install.sh,v 1.46 1999/04/01 21:24:22 deraadt Exp $
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
FQDN=""					# domain name

trap "umount /tmp > /dev/null 2>&1" 0

MODE="install"

# include machine-dependent functions
# The following functions must be provided:
#	md_copy_kernel()	- copy a kernel to the installed disk
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

	while : ; do
		if [ "X${ROOTDISK}" = "X" ]; then
			while [ "X${ROOTDISK}" = "X" ]; do
				getrootdisk
			done
			DISK=$ROOTDISK
		else
			DISK=""
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
		echo	""
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
				_mount_points[${_i}]=""
			fi
			_i=$(( ${_i} + 1 ))
		done
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

if [ -f /sbin/swapon ]; then
	swapon /dev/${ROOTDISK}b
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
			echo -n "Enter system hostname (short form, ie. \"foo\"): [$_nam] "
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
			echo -n "Enter DNS domain name (ie. \"bar.com\"): [$FQDN] "
			getresp "$FQDN"
		done
		FQDN=$resp

		echo ""
		echo "If you have any devices being configured by a DHCP server"
		echo "it is recommended that you do not enter a default route or"
		echo "any name servers."
		echo ""

		configurenetwork

		resp=`route -n show |
		    grep '^default' |
		    sed -e 's/^default          //' -e 's/ .*//'`
		if [ "X${resp}" = "X" ]; then
			resp=none
			if [ -f /tmp/mygate ]; then
				resp=`cat /etc/mygate`
				if [ "X${resp}" = "X" ]; then
					resp="none";
				fi
			fi
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
		if [ -f /etc/resolv.conf ]; then
			resp=""
			for n in `grep '^nameserver ' /etc/resolv.conf | \
			    sed -e 's/^nameserver //'`; do
				if [ "X${resp}" = "X" ]; then
					resp="$n"
				else
					resp="$resp $n"
				fi
			done
		elif [ -f /tmp/resolv.conf ]; then
			resp=""
			for n in `grep '^nameserver ' /tmp/resolv.conf | \
			    sed -e 's/^nameserver //'`; do
				if [ "X${resp}" = "X" ]; then
					resp="$n"
				else
					resp="$resp $n"
				fi
			done
		fi
		echo -n	"Enter IP address of primary nameserver: [$resp] "
		getresp "$resp"
		if [ "X${resp}" != X"none" ]; then
			echo "search $FQDN" > /tmp/resolv.conf
			for n in `echo ${resp}`; do
				echo "nameserver $n" >> /tmp/resolv.conf
			done
			echo "lookup file bind" >> /tmp/resolv.conf

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
		cat << __hosts_table_1

You may want to edit the host table in the event that you are doing an
NFS installation or an FTP installation without a name server and want
to refer to the server by name rather than by its numeric ip address.
__hosts_table_1
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

resp=""		# force one iteration
echo
echo 'Please enter the initial password that the root acount will have.'
while [ "X${resp}" = X"" ]; do
	echo -n "Password (will not echo): "
	stty -echo
	getresp "${_password}"
	stty echo
	echo ""
	_password=$resp

	echo -n "Password (again): "
	stty -echo
	getresp ""
	stty echo
	echo ""
	if [ "${_password}" != "${resp}" ]; then
		echo "Passwords do not match, try again."
		resp=""
	fi
done

install_sets $THESETS

md_copy_kernel

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

if [ -f /etc/dhclient.conf ]; then
	echo -n "Modifying dhclient.conf..."
	cat /etc/dhclient.conf >> /mnt/etc/dhclient.conf
fi

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


md_installboot ${ROOTDISK}

if [ ! -x /mnt/dev/MAKEDEV ]; then
	echo "No /dev/MAKEDEV installed, something is wrong here..."
	exit
fi

echo -n "Making all device nodes (by running /dev/MAKEDEV all) ..."
cd /mnt/dev
sh MAKEDEV all
echo "... done."
cd /

_encr=`echo ${_password} | /mnt/usr/bin/encrypt -b 7`
echo "1,s@^root::@root:${_encr}:@
w
q" | ed /mnt/etc/master.passwd 2> /dev/null
/mnt/usr/sbin/pwd_mkdb -p -d /mnt/etc /etc/master.passwd

unmount_fs /tmp/fstab.shadow

# Pat on the back.
md_congrats

# ALL DONE!
exit 0
