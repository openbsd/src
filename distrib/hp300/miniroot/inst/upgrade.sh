#!/bin/sh
#	$NetBSD: upgrade.sh,v 1.2 1995/11/28 23:57:19 jtc Exp $
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
export VERSION				# XXX needed in subshell
ROOTDISK=""				# filled in below

trap "umount /tmp > /dev/null 2>&1" 0

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

twiddle() {
# spin the propeller so we don't get bored
	while : ; do  
		sleep 1; echo -n "/";
		sleep 1; echo -n "-";
		sleep 1; echo -n "\\";
		sleep 1; echo -n "|";
	done > /dev/tty & echo $!
}

#
# machine dependent section
#
md_get_diskdevs() {
	# return available disk devices
	dmesg | grep "^rd.*:" | awk -F: '{print $1}' | sort -u
	dmesg | grep "^sd.*:*cylinders" | awk -F: '{print $1}' | sort -u
}

md_get_cddevs() {
	# return available CD-ROM devices
	dmesg | grep "sd.*:*CD-ROM" | awk -F: '{print $1}' | sort -u
}

md_installboot() {
	# $1 is the root disk

	echo -n "Installing boot block..."
	disklabel -W ${1}
	disklabel -B ${1}
	echo "done."
}

md_checkfordisklabel() {
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

	# Note, while they might not seem machine-dependent, the
	# welcome banner and the punt message may contain information
	# and/or instructions specific to the type of machine.

md_welcome_banner() {
(
	echo	""
	echo	"Welcome to the NetBSD/hp300 ${VERSION} upgrade program."
	cat << \__welcome_banner_1

This program is designed to help you upgrade your NetBSD system in a
simple and rational way.

As a reminder, installing the `etc' binary set is NOT recommended.
Once the rest of your system has been upgraded, you should manually
merge any changes to files in the `etc' set into those files which
already exist on your system.

As with anything which modifies your hard disk's contents, this
program can cause SIGNIFICANT data loss, and you are advised
to make sure your hard drive is backed up before beginning the
installation process.

Default answers are displyed in brackets after the questions.
You can hit Control-C at any time to quit, but if you do so at a
prompt, you may have to hit return.  Also, quitting in the middle of
installation may leave your system in an inconsistent state.

__welcome_banner_1
) | more
}

md_not_going_to_install() {
		cat << \__not_going_to_install_1

OK, then.  Enter 'halt' at the prompt to halt the machine.  Once the
machine has halted, power-cycle the system to load new boot code.

__not_going_to_install_1
}

md_congrats() {
	cat << \__congratulations_1

CONGRATULATIONS!  You have successfully upgraded NetBSD!  To boot the
installed system, enter halt at the command prompt.  Once the system has
halted, power-cycle the machine in order to load new boot code.  Make sure
you boot from the root disk.

__congratulations_1
}

# end of machine dependent section

do_mfs_mount() {
	# $1 is the mount point
	# $2 is the size in DEV_BIZE blocks

	umount $1 > /dev/null 2>&1
	if ! mount_mfs -s $2 swap $1 ; then
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
		echo "get ${_ftp_file} |\"tar --unlink -zxvpf -\"" >> \
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
	cat $_common_filename | (cd /mnt; tar --unlink -zxvpf -)
	echo "Extraction complete."
}

install_cdrom() {
	# Get the cdrom device info
	cat << \__install_cdrom_1

The following CD-ROM devices are installed on your system; please select
the CD-ROM device containing the installation media:

__install_cdrom_1
	_CDDEVS=`md_get_cddevs`
	echo    "$_CDDEVS"
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
					dd if=$TAPE | tar --unlink -zxvpf -
				)
				;;

			2)
				(
					cd /mnt
					dd if=$TAPE | tar --unlink -xvpf -
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
cat << \__get_timezone_1

Select a time zone:

__get_timezone_1
	ls /usr/share/zoneinfo	# XXX
	echo	""
	if [ X"$TZ" = "X" ]; then
		TZ=`ls -l /etc/timezone 2>/dev/null | awk -F/ '{print $NF}'`
	fi
	echo -n "What timezone are you in [$TZ]? "
	getresp "$TZ"
	case "$resp" in
	"")
		echo "Timezone defaults to GMT"
		TZ="GMT"
		;;
	*)
		TZ="$resp"
		;;
	esac
	export TZ
}

# Much of this is gratuitously stolen from /etc/netstart.
enable_network() {

	# Set up the hostname.
	if [ ! -f /mnt/etc/myname ]; then
		echo "ERROR: no /etc/myname!"
		return 1
	fi
	hostname=`cat /mnt/etc/myname`
	hostname $hostname

	# configure all the interfaces which we know about.
(
	tmp="$IFS"
	IFS="$IFS."
	set -- `echo /mnt/etc/hostname*`
	IFS=$tmp
	unset tmp

	while [ $# -ge 2 ] ; do
		shift		# get rid of "hostname"
		(
			read af name mask bcaddr extras
			read dt dtaddr

			if [ ! -n "$name" ]; then
		    echo "/etc/hostname.$1: invalid network configuration file"
				exit
			fi

			cmd="ifconfig $1 $af $name "
			if [ "${dt}" = "dest" ]; then cmd="$cmd $dtaddr"; fi
			if [ -n "$mask" ]; then cmd="$cmd netmask $mask"; fi
			if [ -n "$bcaddr" -a "X$bcaddr" != "XNONE" ]; then
				cmd="$cmd broadcast $bcaddr";
			fi
			cmd="$cmd $extras"

			$cmd
		) < /mnt/etc/hostname.$1
		shift
	done
)

	# set the address for the loopback interface
	ifconfig lo0 inet localhost

	# use loopback, not the wire
	route add $hostname localhost

	# /etc/mygate, if it exists, contains the name of my gateway host
	# that name must be in /etc/hosts.
	if [ -f /mnt/etc/mygate ]; then
		route delete default > /dev/null 2>&1
		route add default `cat /mnt/etc/mygate`
	fi

	# enable the resolver, if appropriate.
	if [ -f /mnt/etc/resolv.conf ]; then
		_resolver_enabled="TRUE"
		cp /mnt/etc/resolv.conf /tmp/resolv.conf.shadow
	fi

	# Display results...
	echo	"Network interface configuration:"
	ifconfig -a

	echo	""

	if [ "X${_resolver_enabled}" = X"TRUE" ]; then
		netstat -r
		echo	""
		echo	"Resolver enabled."
	else
		netstat -rn
		echo	""
		echo	"Resolver not enabled."
	fi

	return 0
}

# Good {morning,afternoon,evening,night}.
md_welcome_banner
echo -n "Proceed with upgrade? [n] "
getresp "n"
case "$resp" in
	y*|Y*)
		echo	"Cool!  Let's get to it..."
		;;
	*)
		md_not_going_to_install
		exit
		;;
esac

# XXX Work around vnode aliasing bug (thanks for the tip, Chris...)
ls -l /dev > /dev/null 2>&1

# We don't like it, but it sure makes a few things a lot easier.
do_mfs_mount "/tmp" "2048"

while [ "X${ROOTDISK}" = "X" ]; do
	getrootdisk
done

# Make sure there's a disklabel there.  If there isn't, puke after
# disklabel prints the error message.
md_checkfordisklabel ${ROOTDISK}
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

# Assume partition 'a' of $ROOTDISK is for the root filesystem.  Confirm
# this with the user.  Check and mount the root filesystem.
resp=""			# force one iteration
while [ "X${resp}" = "X" ]; do
	echo -n	"Root filesystem? [${ROOTDISK}a] "
	getresp "${ROOTDISK}a"
	_root_filesystem="/dev/`basename $resp`"
	if [ ! -b ${_root_filesystem} ]; then
		echo "Sorry, ${resp} is not a block device."
		resp=""	# force loop to repeat
	fi
done

echo	"Checking root filesystem..."
if ! fsck -pf ${_root_filesystem}; then
	echo	"ERROR: can't check root filesystem!"
	exit 1
fi

echo	"Mounting root filesystem..."
if ! mount -o ro ${_root_filesystem} /mnt; then
	echo	"ERROR: can't mount root filesystem!"
	exit 1
fi

# Grab the fstab so we can munge it for our own use.
if [ ! -f /mnt/etc/fstab ]; then
	echo	"ERROR: no /etc/fstab!"
	exit 1
fi
cp /mnt/etc/fstab /tmp/fstab

# Grab the hosts table so we can use it.
if [ ! -f /mnt/etc/hosts ]; then
	echo	"ERROR: no /etc/hosts!"
	exit 1
fi
cp /mnt/etc/hosts /tmp/hosts

# Start up the network in same/similar configuration as the installed system
# uses.
cat << \__network_config_1

The upgrade program would now like to enable the network.  It will use the
configuration already stored on the root filesystem.  This is required
if you wish to use the network installation capabilities of this program.

__network_config_1
echo -n	"Enable network? [y] "
getresp "y"
case "$resp" in
	y*|Y*)
		if ! enable_network; then
			echo "ERROR: can't enable network!"
			exit 1
		fi

		cat << \__network_config_2

You will now be given the opportunity to escape to the command shell to
do any additional network configuration you may need.  This may include
adding additional routes, if needed.  In addition, you might take this
opportunity to redo the default route in the event that it failed above.

__network_config_2
		echo -n "Escape to shell? [n] "
		getresp "n"
		case "$resp" in
			y*|Y*)
				echo "Type 'exit' to return to upgrade."
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
# fstab.  We remove all but ufs/ffs/nfs.
(
	rm -f /tmp/fstab.new
	while read line; do
		_fstype=`echo $line | awk '{print $3}'`
		if [ "X${_fstype}" = X"ufs" -o \
		    "X${_fstype}" = X"ffs" -o \
		    "X${_fstype}" = X"nfs" ]; then
			echo $line >> /tmp/fstab.new
		fi
	done
) < /tmp/fstab

if [ ! -f /tmp/fstab.new ]; then
	echo	"ERROR: strange fstab!"
	exit 1
fi

# Convert ufs to ffs.
sed -e 's/ufs/ffs/' < /tmp/fstab.new > /tmp/fstab
rm -f /tmp/fstab.new

echo	"The fstab is configured as follows:"
echo	""
cat /tmp/fstab
cat << \__fstab_config_1

You may wish to edit the fstab.  For example, you may need to resolve
dependencies in the order which the filesystems are mounted.  Note that
this fstab is only for installation purposes, and will not be copied into
the root filesystem.

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
if ! umount /mnt; then
	echo	"ERROR: can't unmount previously mounted root!"
	exit 1
fi

# Check all of the filesystems.
echo	"Checking filesystems..."
if ! fsck -pf; then
	echo	"ERROR: can't check filesystems!"
	exit 1
fi

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
and restart the upgrade process.

__mount_filesystems_1
	exit
fi

# Ask the user which media to load the distribution from.
cat << \__install_sets_1

It is now time to extract the installation sets onto the hard disk.
Make sure The sets are either on a local device (i.e. tape, CD-ROM) or on a
network server.

__install_sets_1
if [ -f /base.tar.gz ]; then
	echo -n "Install from sets in the current root filesystem? [y] "
	getresp "y"
	case "$resp" in
		y*|Y*)
			for _f in /*.tar.gz; do
				echo -n "Install $_f ? [y]"
				getresp "y"
				case "$resp" in
				y*|Y*)
				     cat $_f | (cd /mnt; tar --unlink -zxvpf -)
					_yup="TRUE"
					;;
				*)
					;;
				esac
				echo "Extraction complete."
			done
			;;
		*)
			_yup="FALSE"
			;;
	esac
else
	_yup="FALSE"
fi

# Go on prodding for alternate locations
resp=""		# force at least one iteration
while [ "X${resp}" = X"" ]; do
	# If _yup is not FALSE, it means that we extracted sets above.
	# If that's the case, bypass the menu the first time.
	if [ X"$_yup" = X"FALSE" ]; then
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
	else
		_yup="FALSE"	# So we'll ask next time
	fi

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

# Get timezone info
get_timezone

# Fix up the fstab.
echo -n	"Converting ufs to ffs in /etc/fstab..."
sed -e 's/ufs/ffs/' < /mnt/etc/fstab > /tmp/fstab
echo	"done."
echo -n	"Would you like to edit the resulting fstab? [y] "
getresp "y"
case "$resp" in
	y*|Y*)
		vi /tmp/fstab
		;;

	*)
		;;
esac

# Copy in configuration information and make devices in target root.
(
	cd /tmp
	for file in fstab; do
		if [ -f $file ]; then
			echo -n "Copying $file..."
			cp $file /mnt/etc/$file
			echo "done."
		fi
	done

	echo -n "Installing timezone link..."
	rm -f /mnt/etc/localtime
	ln -s /usr/share/zoneinfo/$TZ /mnt/etc/localtime
	echo "done."

	echo -n "Making devices..."
	pid=`twiddle`
	cd /mnt/dev
	sh MAKEDEV all
	kill $pid
	echo "done."

	echo -n "Copying kernel..."
	cp /netbsd /mnt/netbsd
	echo "done."

	md_installboot ${ROOTDISK}
)

# Unmount all filesystems and check their integrity.
echo -n	"Syncing disks..."
pid=`twiddle`
sync; sleep 4; sync; sleep 2; sync; sleep 2
kill $pid
echo	"done."

echo "Unmounting filesystems..."
umount -va

echo "Checking filesystem integrity..."
fsck -pf

# Pat on the back.
md_congrats

# ALL DONE!
exit 0
