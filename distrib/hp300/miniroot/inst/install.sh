#!/bin/sh
#	$NetBSD: install.sh,v 1.1 1995/10/03 22:48:00 thorpej Exp $
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

VERSION=1.0A
ROOTDISK=""				# filled in below
FILESYSTEMS="/tmp/filesystems"		# used thoughout
FQDN=""					# domain name

trap "umount /tmp > /dev/null 2>&1" 0

getresp() {
	read resp
	if [ "X$resp" = "X" ]; then
		resp=$1
	fi
}

do_mfs_mount() {
	umount $1 > /dev/null 2>&1
	if ! mount_mfs -s 2048 swap $1 ; then
		cat << \__mfs_failed_1

FATAL ERROR: Can't mount the memory filesystem.

__mfs_failed_1
		exit
	fi

	# Bleh.  Give mount_mfs a chance to DTRT.
	sleep 2
}

getrootdisk() {
	cat << \__getrootdisk_1

The installation program needs to know which disk to consider
the root disk.  Note the unit number may be different than
the unit number you used in the standalone installation
program.

Available disks are:

__getrootdisk_1
	dmesg | grep "^rd" | grep "slave"
	dmesg | grep "^sd" | grep "slave"
	echo	""
	echo -n	"Which disk is the root disk? "
	getresp ""
	if dmesg | grep "^$resp " | grep "slave" > /dev/null ; then
		ROOTDISK="$resp"
	else
		echo ""
		echo "The disk $resp does not exist."
		ROOTDISK=""
	fi
}

checkfordisklabel() {
	disklabel -r $1 > /dev/null 2> /tmp/checkfordisklabel
	if grep "no disk label" /tmp/checkfordisklabel; then
		rval="1"
	elif grep "disk label corrupted" /tmp/checkfordisklabel; then
		rval="2"
	else
		rval="0"
	fi

	rm -f /tmp/checkfordisklabel
}

labelmoredisks() {
	cat << \__labelmoredisks_1

You may label the following disks:

__labelmoredisks_1
	dmesg | grep "^rd" | grep "slave" | grep -v "${ROOTDISK} "
	dmesg | grep "^sd" | grep "slave" | grep -v "${ROOTDISK} "
	echo	""
	echo -n	"Label which disk? [done] "
	getresp "done"
	case "$resp" in
		done)
			;;

		*)
			if dmesg | grep "^$resp " | grep "slave" \
			    > /dev/null ; then
				# XXX CODE ME
				echo "Yup, it exists."
			else
				echo ""
				echo "The disk $resp does not exist."
			fi
			;;
	esac
}

addhostent() {
	# $1 - IP address
	# $2 - symbolic name

	# Create an entry in the hosts table.  If no host table
	# exists, create one.  If the IP address already exists,
	# replace it's entry.
	if [ ! -f /tmp/hosts ]; then
		echo "127.0.0.1 localhost" > /tmp/hosts
	fi

	if grep "^$1 " /tmp/hosts > /dev/null; then
		grep -v "^$1 " /tmp/hosts > /tmp/hosts.new
		mv /tmp/hosts.new /tmp/hosts
	fi

	echo "$1 $2 $2.$FQDN" >> /tmp/hosts
}

addifconfig() {
	# $1 - interface name
	# $2 - interface symbolic name
	# $3 - interface IP address
	# $4 - interface netmask

	# Create a hostname.* file for the interface.
	echo "inet $2 $4" > /tmp/hostname.$1

	addhostent $3 $2
}

configurenetwork() {
	cat << \__configurenetwork_1

You may configure the following network interfaces:

__configurenetwork_1

	dmesg | grep "^le" | grep "ipl"
	echo	""
	echo -n	"Configure which interface? [done] "
	getresp "done"
	case "$resp" in
		done)
			;;

		*)
			if dmesg | grep "^$resp " | grep "^le" | grep "ipl" \
			    > /dev/null ; then
				_interface_name=$resp

				# Get IP address
				resp=""		# force one iteration
				while [ "X${resp}" = X"" ]; do
					echo -n "IP address? "
					getresp ""
					_interface_ip=$resp
				done

				# Get symbolic name
				resp=""		# force one iteration
				while [ "X${resp}" = X"" ]; do
					echo -n "Symbolic name? "
					getresp ""
					_interface_symname=$resp
				done

				# Get netmask
				resp=""		# force one iteration
				while [ "X${resp}" = X"" ]; do
					echo -n "Netmask? "
					getresp ""
					_interface_mask=$resp
				done

				# Configure the interface.  If it
				# succeeds, add it to the permanent
				# network configuration info.
				ifconfig ${_interface_name} down
				if ifconfig ${_interface_name} inet \
				    ${_interface_ip} \
				    netmask ${_interface_mask} up ; then
					addifconfig \
					    ${_interface_name} \
					    ${_interface_symname} \
					    ${_interface_ip} \
					    ${_interface_mask}
				fi
			else
				echo ""
				echo "The interface $resp does not exist."
			fi
			;;
	esac
}

install_ftp() {
	# Get several parameters from the user, and create
	# a shell script that directs the appropriate
	# commands into ftp.
	cat << \__install_ftp_1

This is an automated ftp-based installation process.  You will be asked
several questions.  The correct set of commands will be placed in a script
that will be fed to ftp(1).

__install_ftp_1
	# Get server IP address
	resp=""		# force one iteration
	while [ "X${resp}" = X"" ]; do
		echo -n "Server IP? [${_ftp_server_ip}] "
		getresp "${_ftp_server_ip}"
		_ftp_server_ip=$resp
	done

	# Get server directory
	resp=""		# force one iteration
	while [ "X${resp}" = X"" ]; do
		echo -n "Server directory? [${_ftp_server_dir}] "
		getresp "${_ftp_server_dir}"
		_ftp_server_dir=$resp
	done

	# Get login name
	resp=""		# force one iteration
	while [ "X${resp}" = X"" ]; do
		echo -n "Login? [${_ftp_server_login}] "
		getresp "${_ftp_server_login}"
		_ftp_server_login=$resp 
	done

	# Get password
	resp=""		# force one iteration
	while [ "X${resp}" = X"" ]; do
		echo -n "Password? [${_ftp_server_password}] "
		getresp "${_ftp_server_password}"
		_ftp_server_password=$resp
	done

	# Get list of files for mget.
	cat << \__install_ftp_2

You will now be asked for files to extract.  Enter one file at a time.
When you are done entering files, enter 'done'.

__install_ftp_2
	echo "#!/bin/sh" > /tmp/ftp-script.sh
	echo "cd /mnt" >> /tmp/ftp-script.sh
	echo "ftp -i -n $_ftp_server_ip << \__end_commands" >> \
	    /tmp/ftp-script.sh
	echo "user $_ftp_server_login $_ftp_server_password" >> \
	    /tmp/ftp-script.sh
	echo "bin" >> /tmp/ftp-script.sh
	echo "cd $_ftp_server_dir" >> /tmp/ftp-script.sh

	resp=""		# force one interation
	while [ "X${resp}" != X"done" ]; do
		echo -n "File? [done] "
		getresp "done"
		if [ "X${resp}" = X"done" ]; then
			break
		fi

		_ftp_file=`echo ${resp} | awk '{print $1}'`
		echo "get ${_ftp_file} |\"tar -zxvpf -\"" >> \
		    /tmp/ftp-script.sh
	done

	echo "quit" >> /tmp/ftp-script.sh
	echo "__end_commands" >> /tmp/ftp-script.sh

	sh /tmp/ftp-script.sh
	rm -f /tmp/ftp-script.sh
	echo "Extraction complete."
}

install_common_nfs_cdrom() {
	# $1 - directory containing file

	# Get the name of the file.
	resp=""		# force one iteration
	while [ "X${resp}" = X"" ]; do
		echo -n "File name? "
		getresp ""
	done
	_common_filename="/mnt2/$1/$resp"

	# Ensure file exists
	if [ ! -f $_common_filename ]; then
		echo "File $_common_filename does not exist.  Check to make"
		echo "sure you entered the information properly."
		return
	fi

	# Extract file
	cat $_common_filename | (cd /mnt; tar -zxvpf -)
	echo "Extraction complete."
}

install_cdrom() {
	# Get the cdrom device info
	cat << \__install_cdrom_1

The following SCSI disk or disk-like devices are installed on your system;
please select the CD-ROM device containing the installation media:

__install_cdrom_1
	dmesg | grep "^sd" | grep "rev"
	echo	""
	echo -n	"Which is the CD-ROM with the installation media? [abort] "
	getresp "abort"
	case "$resp" in
		abort)
			echo "Aborting."
			return
			;;

		*)
			if dmesg | grep "^$resp " | grep "slave" \
			    > /dev/null ; then
				_cdrom_drive=$resp
			else
				echo ""
				echo "The CD-ROM $resp does not exist."
				echo "Aborting."
				return
			fi
			;;
	esac

	# Get partition
	resp=""		# force one iteration
	while [ "X${resp}" = X"" ]; do
		echo -n "Partition? [c] "
		getresp "c"
		case "$resp" in
			[a-h])
				_cdrom_partition=$resp
				;;

			*)
				echo "Invalid response: $resp"
				resp=""		# force loop to repeat
				;;
		esac
	done

	# Ask for filesystem type
	cat << \__install_cdrom_2

There are two CD-ROM filesystem types currently supported by this program:
	1) ISO-9660 (cd9660)
	2) Berkeley Fast Filesystem (ffs)

__install_cdrom_2
	resp=""		# force one iteration
	while [ "X${resp}" = X"" ]; do
		echo -n "Which filesystem type? [cd9660] "
		getresp "cd9660"
		case "$resp" in
			cd9660|ffs)
				_cdrom_filesystem=$resp
				;;

			*)
				echo "Invalid response: $resp"
				resp=""		# force loop to repeat
				;;
		esac
	done

	# Mount the CD-ROM
	if ! mount -t ${_cdrom_filesystem} -o ro \
	    /dev/${_cdrom_drive}${_cdrom_partition} /mnt2 ; then
		echo "Cannot mount CD-ROM drive.  Aborting."
		return
	fi

	# Get the directory where the file lives
	resp=""		# force one iteration
	while [ "X${resp}" = X"" ]; do
		echo "Enter the directory relative to the mount point that"
		echo -n "contains the file. [${_cdrom_directory}] "
		getresp "${_cdrom_directory}"
	done
	_cdrom_directory=$resp

	install_common_nfs_cdrom ${_cdrom_directory}
	umount -f /mnt2 > /dev/null 2>&1
}

install_nfs() {
	# Get the IP address of the server
	resp=""		# force one iteration
	while [ "X${resp}" = X"" ]; do
		echo -n "Server IP address? [${_nfs_server_ip}] "
		getresp "${_nfs_server_ip}"
	done
	_nfs_server_ip=$resp

	# Get server path to mount
	resp=""		# force one iteration
	while [ "X${resp}" = X"" ]; do
		echo -n "Filesystem on server to mount? [${_nfs_server_path}] "
		getresp "${_nfs_server_path}"
	done
	_nfs_server_path=$resp

	# Determine use of TCP
	echo -n "Use TCP transport (only works with capable NFS server)? [n] "
	getresp "n"
	case "$resp" in
		y*|Y*)
			_nfs_tcp="-T"
			;;

		*)
			_nfs_tcp=""
			;;
	esac

	# Mount the server
	if ! mount_nfs $_nfs_tcp ${_nfs_server_ip}:${_nfs_server_path} \
	    /mnt2 ; then
		echo "Cannot mount NFS server.  Aborting."
		return
	fi

	# Get the directory where the file lives
	resp=""		# force one iteration
	while [ "X${resp}" = X"" ]; do
		echo "Enter the directory relative to the mount point that"
		echo -n "contains the file. [${_nfs_directory}] "
		getresp "${_nfs_directory}"
	done
	_nfs_directory=$resp

	install_common_nfs_cdrom ${_nfs_directory}
	umount -f /mnt2 > /dev/null 2>&1
}

install_tape() {
	# Get the name of the tape from the user.
	cat << \__install_tape_1

The installation program needs to know which tape device to use.  Make
sure you use a "no rewind on close" device.

__install_tape_1
	_tape=`basename $TAPE`
	resp=""		# force one iteration
	while [ "X${resp}" = X"" ]; do
		echo -n "Name of tape device? [${_tape}]"
		getresp "${_tape}"
	done
	_tape=`basename $resp`
	TAPE="/dev/${_tape}"
	if [ ! -c $TAPE ]; then
		echo "$TAPE does not exist or is not a character special file."
		echo "Aborting."
		return
	fi
	export TAPE

	# Rewind the tape device
	echo -n "Rewinding tape..."
	if ! mt rewind ; then
		echo "$TAPE may not be attached to the system or may not be"
		echo "a tape device.  Aborting."
		return
	fi
	echo "done."

	# Get the file number
	resp=""		# force one iteration
	while [ "X${resp}" = X"" ]; do
		echo -n "File number? "
		getresp ""
		case "$resp" in
			[1-9]*)
				_nskip=`expr $resp - 1`
				;;

			*)
				echo "Invalid file number ${resp}."
				resp=""		# fore loop to repeat
				;;
		esac
	done

	# Skip to correct file.
	echo -n "Skipping to source file..."
	if [ "X${_nskip}" != X"0" ]; then
		if ! mt fsf $_nskip ; then
			echo "Could not skip $_nskip files.  Aborting."
			return
		fi
	fi
	echo "done."

	cat << \__install_tape_2

There are 2 different ways the file can be stored on tape:

	1) an image of a gzipped tar file
	2) a standard tar image

__install_tape_2
	resp=""		# force one iteration
	while [ "X${resp}" = X"" ]; do
		getresp "1"
		case "$resp" in
			1)
				(
					cd /mnt
					dd if=$TAPE | tar -zxvpf -
				)
				;;

			2)
				(
					cd /mnt
					tar -zxvpf $TAPE
				)
				;;

			*)
				echo "Invalid response: $resp."
				resp=""		# force loop to repeat
				;;
		esac
	done
	echo "Extraction complete."
}

echo	""
echo	"Welcome to the NetBSD ${VERSION} installation program."
cat << \__welcome_banner_1

This program is designed to help you put NetBSD on your hard disk,
in a simple and rational way.  You'll be asked several questions,
and it would probably be useful to have your disk's hardware
manual, the installation notes, and a calculator handy.

In particular, you will need to know some reasonably detailed
information about your disk's geometry.  This program can determine
some limited information about certain specific types of HP-IB disks.
If you have SCSI disks, however, prior knowledge of disk geometry
is absolutely essential.

As with anything which modifies your hard disk's contents, this
program can cause SIGNIFICANT data loss, and you are advised
to make sure your hard drive is backed up before beginning the
installation process.

Default answers are displyed in brackets after the questions.
You can hit Control-C at any time to quit, but if you do so at a
prompt, you may have to hit return.  Also, quitting in the middle of
installation may leave your system in an inconsistent state.

__welcome_banner_1
echo -n "Proceed with installation? [n] "
getresp "n"
case "$resp" in
	y*|Y*)
		echo	"Cool!  Let's get to it..."
		;;
	*)
		cat << \__welcome_banner_2

OK, then.  Enter 'halt' at the prompt to halt the machine.  Once the
machine has halted, power-cycle the system to load new boot code.

__welcome_banner_2
		exit
		;;
esac

# We don't like it, but it sure makes a few things a lot easier.
do_mfs_mount "/tmp"

# Install the shadowed disktab file; lets us write to it for temporary
# purposes without mounting the miniroot read-write.
cp /etc/disktab.shadow /tmp/disktab.shadow

while [ "X${ROOTDISK}" = "X" ]; do
	getrootdisk
done

# Make sure there's a disklabel there.  If there isn't, puke after
# disklabel prints the error message.
checkfordisklabel ${ROOTDISK}
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

# Give the user the opportinuty to edit the root disklabel.
cat << \__disklabel_notice_1

You have already placed a disklabel onto the target root disk.
However, due to the limitations of the standalone program used
you may want to edit that label to change partition type information.
You will be given the opporunity to do that now.  Note that you may
not change the size or location of any presently open partition.

__disklabel_notice_1
echo -n	"Do you wish to edit the root disklabel? [y] "
getresp "y"
case "$resp" in
	y*|Y*)
		disklabel -W ${ROOTDISK}
		disklabel -e ${ROOTDISK}
		;;

	*)
		;;
esac

cat << \__disklabel_notice_2

You will now be given the opportunity to place disklabels on any additional
disks on your system.
__disklabel_notice_2

resp="X"	# force at least one iteration
while [ "X$resp" != X"done" ]; do
	labelmoredisks
done

# Assume partition 'a' of $ROOTDISK is for the root filesystem.  Loop and
# get the rest.
# XXX ASSUMES THAT THE USER DOESN'T PROVIDE BOGUS INPUT.
cat << \__get_filesystems_1

You will now have the opportunity to enter filesystem information.
You will be prompted for device name and mount point (full path,
including the prepending '/' character).

Note that these do not have to be in any particular order.  You will
be given the opportunity to edit the resulting 'fstab' file before
any of the filesystems are mounted.  At that time you will be able
to resolve any filesystem order dependencies.

__get_filesystems_1

echo	"The following will be used for the root filesystem:"
echo	"	${ROOTDISK}a	/"

echo	"${ROOTDISK}a	/" > ${FILESYSTEMS}

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
				if [ "X${_mount_point}" = X"/" ]; then
					# Invalid response; no multiple roots
					_first_char="X"
				else
					_first_char=`echo ${_mount_point} | \
					    cut -c 1`
				fi
			done
			echo "${_device_name}	${_mount_point}" >> \
			    ${FILESYSTEMS}
			resp="X"	# force loop to repeat
			;;
	esac
done

echo	""
echo	"You have configured the following devices and mount points:"
echo	""
cat ${FILESYSTEMS}
echo	""
echo	"Filesystems will now be created on these devices.  If you made any"
echo -n	"mistakes, you may edit this now.  Edit? [n] "
getresp "n"
case "$resp" in
	y*|Y*)
		vi ${FILESYSTEMS}
		;;
	*)
		;;
esac

# Loop though the file, place filesystems on each device.
echo	"Creating filesystems..."
(
	while read line; do
		_device_name=`echo $line | awk '{print $1}'`
		newfs /dev/r${_device_name}
		echo ""
	done
) < ${FILESYSTEMS}

# Get network configuration information, and store it for placement in the
# root filesystem later.
cat << \__network_config_1
You will now be given the opportunity to configure the network.  This will
be useful if you need to transfer the installation sets via FTP or NFS.
Even if you choose not to transfer installation sets that way, this
information will be preserved and copied into the new root filesystem.

Note, enter all symbolic host names WITHOUT the domain name appended.
I.e. use 'hostname' NOT 'hostname.domain.name'.

__network_config_1
echo -n	"Configure the network? [y] "
getresp "y"
case "$resp" in
	y*|Y*)
		echo -n "Enter system hostname: "
		resp=""		# force at least one iteration
		while [ "X${resp}" = X"" ]; do
			getresp ""
		done
		hostname $resp
		echo $resp > /tmp/myname

		echo -n "Enter DNS domain name: "
		resp=""		# force at least one iteration
		while [ "X${resp}" = X"" ]; do
			getresp ""
		done
		FQDN=$resp

		resp=""		# force at least one iteration
		while [ "X${resp}" != X"done" ]; do
			configurenetwork
		done

		echo -n "Enter IP address of default route: [none] "
		getresp "none"
		if [ "X${resp}" != X"none" ]; then
			route delete default > /dev/null 2>&1
			if route add default $resp > /dev/null ; then
				echo $resp > /tmp/mygate
			fi
		fi

		echo ""
		echo "The host table is as follows:"
		echo ""
		cat /tmp/hosts
		echo ""
		echo "You may want to edit the host table in the event that"
		echo "you need to mount an NFS server."
		echo -n "Would you like to edit the host table? [n] "
		getresp "n"
		case "$resp" in
			y*|Y*)
				vi /tmp/hosts
				;;

			*)
				;;
		esac

		cat << \__network_config_2

You will now be given the opportunity to escape to the command shell to
do any additional network configuration you may need.  This may include
adding additional routes, if needed.  In addition, you might take this
opportunity to redo the default route in the event that it failed above.
If you do change the default route, and wish for that change to carry over
to the installed system, execute the following command at the shell
prompt:

	echo <ip_address_of_gateway> > /tmp/mygate

where <ip_address_of_gateway> is the IP address of the default router.

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

# Now that the network has been configured, it is safe to configure the
# fstab.
awk '{
	if ($2 == "/")
		printf("/dev/%s %s ffs rw 1 1\n", $1, $2)
	else
		printf("/dev/%s %s ffs rw 1 2\n", $1, $2)
}' < ${FILESYSTEMS} > /tmp/fstab

echo	"The fstab is configured as follows:"
echo	""
cat /tmp/fstab
cat << \__fstab_config_1

You may wish to edit the fstab.  For example, you may need to resolve
dependencies in the order which the filesystems are mounted.  You may
also wish to take this opportunity to place NFS mounts in the fstab.
This would be especially useful if you plan to keep '/usr' on an NFS
server.

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

# Now that the 'real' fstab is configured, we munge it into a 'shadow'
# fstab which we'll use for mounting and unmounting all of the target
# filesystems relative to /mnt.  Mount all filesystems.
awk '{
	if ($2 == "/")
		printf("%s /mnt %s %s %s %s\n", $1, $3, $4, $5, $6)
	else
		printf("%s /mnt%s %s %s %s %s\n", $1, $2, $3, $4, $5, $6)
}' < /tmp/fstab > /tmp/fstab.shadow

echo	""

# Must mount filesystems manually, one at a time, so we can make sure the
# mount points exist.
(
	while read line; do
		_dev=`echo $line | awk '{print $1}'`
		_mp=`echo $line | awk '{print $2}'`
		_fstype=`echo $line | awk '{print $3}'`
		_opt=`echo $line | awk '{print $4}'`

		# If not the root filesystem, make sure the mount
		# point is present.
		if [ "X{$_mp}" != X"/mnt" ]; then
			mkdir -p $_mp
		fi

		# Mount the filesystem.  If the mount fails, exit
		# with an error condition to tell the outer
		# later to bail.
		if ! mount -v -t $_fstype -o $_opt $_dev $_mp ; then
			# error message displated by mount
			exit 1
		fi
	done
) < /etc/fstab

if [ "X${?}" != X"0" ]; then
	cat << \__mount_filesystems_1

FATAL ERROR:  Cannot mount filesystems.  Double-check your configuration
and restart the installation process.

__mount_filesystems_1
	exit
fi

# Ask the user which media to load the distribution from.
cat << \__install_sets_1

It is now time to extract the installation sets onto the hard disk.
Make sure The sets are either on a local device (i.e. tape, CD-ROM) or on a
network server.

__install_sets_1
resp=""		# force at least one iteration
while [ "X${resp}" = X"" ]; do
	echo -n	"Install from (f)tp, (t)ape, (C)D-ROM, or (N)FS? [f] "
	getresp "f"
	case "$resp" in
		f*|F*)
			install_ftp
			;;

		t*|T*)
			install_tape
			;;

		c*|C*)
			install_cdrom
			;;

		n*|N*)
			install_nfs
			;;

		*)
			echo "Invalid response: $resp"
			resp=""
			;;
	esac

	# Give the user the opportunity to extract more sets.  They don't
	# necessarily have to come from the same media.
	echo	""
	echo -n	"Extract more sets? [n] "
	getresp "n"
	case "$resp" in
		y*|Y*)
			# Force loop to repeat
			resp=""
			;;

		*)
			;;
	esac
done

# Copy in configuration information and make devices in target root.
(
	cd /tmp
	for file in fstab hostname.* hosts myname mygate; do
		if [ -f $file ]; then
			echo "Copying $file..."
			cp $file /mnt/etc/$file
		fi
	done

	echo -n "Making devices..."
	cd /mnt/dev
	sh MAKEDEV all
	echo "done."

	echo "Copying kernel..."
	cp /netbsd /mnt/netbsd
)

# Unmount all filesystems and check their integrity.
umount -a
echo "Checking filesystem integrity..."
fsck -pf

# Install boot code on target disk.
echo "Installing boot block..."
disklabel -W ${ROOTDISK}
disklabel -B ${ROOTDISK}

cat << \__congratulations_1

CONGRATULATIONS!  You have successfully installed NetBSD on your hard disk!
To boot the installed system, enter halt at the command prompt.  Once the
system has halted, power-cycle the machine in order to load new boot code.
Make sure you boot from the disk.

__congratulations_1

# ALL DONE!
exit
