#!/bin/sh
#	$NetBSD: upgrade.sh,v 1.3 1995/11/01 21:10:41 pk Exp $
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

VERSION=1.1
ROOTDISK=""				# filled in below
FILESYSTEMS="/tmp/filesystems"		# used thoughout
FQDN=""					# domain name

trap "umount /tmp /mnt/usr /mnt > /dev/null 2>&1" 0

getresp() {
	read resp
	if [ "X$resp" = "X" ]; then
		resp=$1
	fi
}

isin() {
# test the first argument against the remaining ones, return succes on a match
	_a=$1; shift
	while [ $# != 0 ]; do
		if [ "$_a" = "$1" ]; then return 0; fi
		shift
	done
	return 1
}

rmel() {
# remove first argument from list formed by the remaining arguments
	_a=$1; shift
	while [ $# != 0 ]; do
		if [ "$_a" != "$1" ]; then echo "$1"; fi
		shift
	done
}

twiddle()
{
	while : ; do
		sleep 1; echo -n "/";
		sleep 1; echo -n "-";
		sleep 1; echo -n "\\";
		sleep 1; echo -n "|";
	 done > /dev/tty & echo $!
}

set_terminal() {
	echo -n "Specify terminal type [sun]: "
	getresp "sun"
	TERM="$resp"
	export TERM
}

#
# machine dependent section
#
md_get_diskdevs() {
	# return available disk devices
	dmesg | grep "^sd.*at scsibus" | cut -d" " -f1
}

md_get_cddevs() {
	# return available CDROM devices
	dmesg | grep "^sd" | grep "rev" | cut -d" " -f1
}

md_get_ifdevs() {
	# return available network devices
	dmesg | egrep "(^le[0-9]|^ie[0-9])" | cut -d" " -f1
}

md_installboot() {
	echo "Installing boot block..."
	/usr/mdec/binstall -v ffs /mnt
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
	_DKDEVS=`md_get_diskdevs`
	echo	"$_DKDEVS"
	echo	""
	echo -n	"Which disk is the root disk? "
	getresp ""
	if isin $resp $_DKDEVS ; then
		ROOTDISK="$resp"
	else
		echo ""
		echo "The disk $resp does not exist."
		ROOTDISK=""
	fi
}

checkfordisklabel() {
	disklabel $1 > /dev/null 2> /tmp/checkfordisklabel
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
	_DKDEV=`rmel "${ROOTDISK}"`
	echo $_DKDEVS
	echo	""
	echo -n	"Label which disk? [done] "
	getresp "done"
	case "$resp" in
		"done")
			;;

		*)
			if echo "$_DKDEVS" | grep "^$resp" > /dev/null ; then
				disklabel -e $resp
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

	_IFS=`md_get_ifdevs`
	echo	$_IFS
	echo	""
	echo -n	"Configure which interface? [done] "
	getresp "done"
	case "$resp" in
		"done")
			;;

		*)
			if isin $resp $_IFS ; then
				_interface_name=$resp

				# remove from list
				_IFS=`rmel $resp "$_IFS"`

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
	_CDDEVS=`md_get_cddevs`
	echo	"$_CDDEVS"
	echo	""
	echo -n	"Which is the CD-ROM with the installation media? [abort] "
	getresp "abort"
	case "$resp" in
		abort)
			echo "Aborting."
			return
			;;

		*)
			if isin $resp $_CDDEVS ; then
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

get_timezone() {
	local _a
cat << \__get_timezone_1

Select a time zone for your location. Timezones are represented on the
system by a directory structure rooted in "/usr/share/timezone". Most
timezones can be selected by entering a token like "MET" or "GMT-6".
Other zones are grouped by continent, with detailed zone information
separated by a slash ("/"), e.g. "US/Pacific".

To get a listing of what's available in /usr/share/timezone, enter "?"
at the first prompt below.

__get_timezone_1
	if [ X$TZ = X ]; then
		TZ=`ls -l /etc/timezone 2>/dev/null | awk '{print $NF}' |
			sed -e 's?/usr/share/timezone/??'`
	fi
	while :; do
		echo -n	"What timezone are you in [$TZ]? "
		getresp "$TZ"
		case "$resp" in
		"")
			echo "Timezone defaults to GMT"
			TZ="GMT"
			break;
			;;
		"?")
			ls /usr/share/zoneinfo
			;;
		*)
			_a=$resp
			if [ -d /usr/share/zoneinfo/$_a ]; then
				echo -n "There are several timezones available"
				echo " within '$_a'"
				echo -n "Select a sub-timezone: "
				getresp ""
				_a=${_a}/${resp}
			fi
			if [ -f /usr/share/zoneinfo/$_a ]; then
				TZ="$_a"
				echo "You have selected timezone "$_a".
				break 2
			fi
			echo "'/usr/share/zoneinfo/$_a' is not a valid timezone on this system."
			;;
		esac
	done
}

echo	""
echo	"Welcome to the NetBSD/sparc ${VERSION} upgrade program."
cat << \__welcome_banner_1

This program is designed to help you put NetBSD on your disk,
in a simple and rational way.  You'll be asked several questions,
and it would probably be useful to have your disk's hardware
manual, the installation notes, and a calculator handy.

As with anything which modifies your disk's contents, this
program can cause SIGNIFICANT data loss, and you are advised
to make sure your data is backed up before beginning the
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

set_terminal

# We don't like it, but it sure makes a few things a lot easier.
##do_mfs_mount "/tmp"

# Install the shadowed disktab file; lets us write to it for temporary
# purposes without mounting the miniroot read-write.
##cp /etc/disktab.shadow /tmp/disktab.shadow

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
label the disk before continuing.

__disklabel_not_present_1
		exit
		;;

	2)
		cat << \__disklabel_corrupted_1

FATAL ERROR: The disklabel on the root disk is corrupted!  You must
re-label the disk before continuing.

__disklabel_corrupted_1
		exit
		;;

	*)
		;;
esac

cat << \__mount_root
Ready to mount your existing root filesystem. This is normally
the `a' partition on your boot disk.

__mount_root

while : ; do
	echo -n	"Root filesystem? [${ROOTDISK}a] "
	getresp "${ROOTDISK}a"
	case "$resp" in
		*)
			mount /dev/$resp /mnt
			if [ $? = 0 ]; then
				break 2;
			fi
			echo "$resp could not be mounted"
			;;
	esac
done

# Look in /mnt/etc/fstab for /usr filesystem.
awk '{
	if ($2 == "/" || $2 == "/usr") {
		print
	}
}' < /mnt/etc/fstab > /tmp/fstab

echo	"These filesystems are configured to be used for this upgrade:"
echo	""
cat /tmp/fstab
cat << \__fstab_config_1

You may wish to edit the fstab.  For example, you may need to resolve
dependencies in the order which the filesystems are mounted.  You may
also wish to take this opportunity to place NFS mounts in the fstab.
This would be especially useful if you plan to keep '/usr' on an NFS
server.

You also need to edit the fstab file if your disk has been assigned a
different unit number by the currently running kernel. For instance,
a SCSI disk that was known as `sd0' in your existing configuration
might appear as `sd3' here. If this is the case, change all old
unit numbers to the new unit number.

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
		if [ "X${_mp}" != X"/mnt" ]; then
			mkdir -p $_mp
			# note: root already mounted on /mnt
		else
			continue;
		fi

		# Mount the filesystem.  If the mount fails, exit
		# with an error condition to tell the outer
		# layer to bail.
		if ! mount -v -t $_fstype -o $_opt $_dev $_mp ; then
			# error message displayed by mount
			exit 1
		fi
	done
) < /tmp/fstab.shadow

if [ "X${?}" != X"0" ]; then
	cat << \__mount_filesystems_1

FATAL ERROR:  Cannot mount filesystems.  Double-check your configuration
and restart the installation process.

__mount_filesystems_1
	exit
fi

# Ask the user which media to load the distribution from.
cat << \__install_sets_1

It is now time to extract the installation sets onto the disk.
Make sure The sets are either on a local device (i.e. tape, CD-ROM) or on a
network server.

__install_sets_1

ALLSETS="base comp etc games man misc text"
UPGRSETS="base comp games man misc text"
RELDIR=
RELDIR=/a/release

if [ -f $RELDIR/base.tar.gz ]; then
	echo -n	"Install from sets in the current root filesystem? [y] "
	getresp "y"
	case "$resp" in
		y*|Y*)
			for _f in $UPGRSETS; do
				echo -n "Install $_f ? [y]"
				getresp "y"
				case "$resp" in
				y*|Y*)
					cat $RELDIR/${_f}.tar.gz |
						(cd /mnt; tar -zxvpf -)
					_yup=X
					;;
				*)
					;;
				esac
				echo "Extraction complete."
			done
			resp="$_yup"
			;;
		*)
			resp=""
			;;
	esac
else
	# Go on prodding for alternate locations
	resp=""		# force at least one iteration
fi

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

#get_timezone

echo -n "Do you want to install the NetBSD bootblocks on your boot disk? [y]"
getresp "y"
case "$resp" in
	y*|Y*)
		_INSTBOOT="Y"
		;;

	*)
		_INSTBOOT="N"
		;;
esac

# Copy in configuration information and make devices in target root.
(
	echo -n "Making devices..."
	cd /mnt/dev
	pid=`twiddle`
	sh MAKEDEV all
	kill $pid
	echo "done."

	if [ -f /mnt/netbsd ]; then
		echo "Saving existing kernel in netbsd.1.0."
		cp /mnt/netbsd /mnt/netbsd.1.0
	fi

	echo "Copying netbsd 1.1 kernel ..."
	cp /netbsd /mnt/netbsd

	if [ "$_INSTBOOT" = "Y" ]; then
		echo "Installing NetBSD bootblock..."
		md_installboot ${ROOTDISK}
	fi
)

# Unmount all filesystems and check their integrity.
(
	_devs=""
	_mps=""
	# maintain reverse order
	while read line; do
		_devs="`echo $line | awk '{print $1}'` ${_devs}"
		_mps="`echo $line | awk '{print $2}'` ${_mps}"
	done
	echo -n "Umounting filesystems... "
	for _mp in ${_mps}; do
		echo -n "${_mp} "
		umount ${_mp}
	done
	echo "Done."

	echo "Checking filesystem integrity..."
	for _dev in ${_devs}; do
		echo  "${_dev}"
		fsck -f ${_dev}
	done
	echo "Done."
) < /tmp/fstab.shadow

##umount -a
##echo "Checking filesystem integrity..."
##fsck -pf

#md_installboot_xxx

cat << \__congratulations_1

CONGRATULATIONS!  You have successfully installed NetBSD!
To boot the installed system, enter halt at the command prompt.  Once the
system has halted, reset the machine and boot from the disk.

__congratulations_1

# ALL DONE!
exit
