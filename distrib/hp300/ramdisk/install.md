#!/bin/sh
#
#	$OpenBSD: install.md,v 1.12 1998/09/11 22:55:45 millert Exp $
#	$NetBSD: install.md,v 1.1.2.4 1996/08/26 15:45:14 gwr Exp $
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

#
# machine dependent section of installation/upgrade script
#

# Machine-dependent install sets
MDSETS="kernel"

TMPWRITEABLE=/tmp/writeable
KERNFSMOUNTED=/tmp/kernfsmounted

md_set_term() {
	echo -n "Specify terminal type [hp300h]: "
	getresp "hp300h"
	TERM="$resp"
	export TERM
	# set screensize (i.e., for an xterm)
	rows=`stty -a | grep rows | awk '{print $4}'`
	columns=`stty -a | grep columns | awk '{print $6}'`
	if [ "$rows" -eq 0 -o "$columns" -eq 0 ]; then
		echo -n "Specify terminal rows [25]: "
		getresp "25"
		rows="$resp"

		echo -n "Specify terminal columns [80]: "
		getresp "80"
		columns="$resp"

		stty rows "$rows" columns "$columns"
	fi
}

md_makerootwritable() {

	if [ -e ${TMPWRITEABLE} ]
	then
		md_mountkernfs
		return
	fi
	if ! mount -t ffs  -u /dev/rd0a / ; then
		cat << \__rd0_failed_1

FATAL ERROR: Can't mount the ram filesystem.

__rd0_failed_1
		exit
	fi

	# Bleh.  Give mount_mfs a chance to DTRT.
	sleep 2
	> ${TMPWRITEABLE}

	md_mountkernfs
}

md_mountkernfs() {
	if [ -e ${KERNFSMOUNTED} ]
	then
		return
	fi
	if ! mount -t kernfs /kern /kern
	then
		cat << \__kernfs_failed_1
FATAL ERROR: Can't mount kernfs filesystem
__kernfs_failed_1
		exit
	fi
	> ${KERNFSMOUNTED} 
}

md_machine_arch() {
	cat /kern/machine
}

md_get_diskdevs() {
	# return available disk devices
	egrep "^hd[0-9]*:." < /kern/msgbuf | cut -d":" -f1 | sort -u
	egrep "^sd[0-9]*:.*cylinders" < /kern/msgbuf | cut -d":" -f1 | sort -u
}

md_get_cddevs() {
	# return available CD-ROM devices
	egrep "sd[0-9]*:.*CD-ROM" < /kern/msgbuf | cut -d":" -f1 | sort -u
}

md_get_partition_range() {
	# return range of valid partition letters
	echo "[a-p]"
}

md_installboot() {
	# $1 is the root disk

	echo -n "Installing boot block..."
	disklabel -W ${1}
	disklabel -B ${1}
	echo "done."
}

md_checkfordisklabel() {
	# $1 is the disk to check
	local rval

	disklabel -r $1 > /dev/null 2> /tmp/checkfordisklabel
	if grep "no disk label" /tmp/checkfordisklabel; then
		rval=1
	elif grep "disk label corrupted" /tmp/checkfordisklabel; then
		rval=2
	else
		rval=0
	fi

	rm -f /tmp/checkfordisklabel
	return $rval
}

hp300_init_label_scsi_disk() {
	# $1 is the disk to label

	# Name the disks we install in the temporary fstab.
	if [ "X${_disk_instance}" = "X" ]; then
		_disk_instance="0"
	else
		_disk_instance=`expr $_disk_instance + 1`
	fi
	_cur_disk_name="install-disk-${_disk_instance}"

	# Get geometry information from the user.
	less << \__scsi_label_1

You will need to provide some information about your disk's geometry.
Geometry info for SCSI disks was printed at boot time.  If that information
is not available, use the information provided in your disk's manual.
Please note that the geometry printed at boot time is preferred.

IMPORTANT NOTE: due to a limitation in the disklabel(8) program, the
number of cylinders on the disk will be increased by 1 so that the initial
label can be placed on disk for editing.  When the disklabel editor appears,
make absolutely certain you subtract 1 from the total number of cylinders,
and adjust the size of partition 'c' such that:

	size = (sectors per track) * (tracks per cyl) * (total cylinders)

Note that the disklabel editor will be run twice; once to set the size of
partition 'c' and correct the geometry, and again so that you may correctly
edit the partition map.  This is to work around the afore mentioned
limitation in disklabel(8).  Apologies offered in advance.

__scsi_label_1

	# Give the opportunity to review the boot messages.
	echo -n	"Review boot messages now? [y] "
	getresp "y"
	case "$resp" in
		y*|Y*)
			less -rsS /kern/msgbuf
			;;

		*)
			;;
	esac

	echo	""
	echo -n	"Number of bytes per disk sector? [512] "
	getresp "512"
	_secsize="$resp"

	resp=""		# force one iteration
	while [ "X${resp}" = "X" ]; do
		echo -n	"Number of cylinders? "
		getresp ""
	done
	_cylinders="$resp"
	_fudge_cyl=`expr $_cylinders + 1`

	resp=""		# force one iteration
	while [ "X${resp}" = "X" ]; do
		echo -n	"Number of tracks (heads)? "
		getresp ""
	done
	_tracks_per_cyl="$resp"

	resp=""		# force one iteration
	while [ "X${resp}" = "X" ]; do
		echo -n	"Number of disk sectors (blocks)? "
		getresp ""
	done
	_nsectors="$resp"

	# Calculate some values we need.
	_sec_per_cyl=`expr $_nsectors / $_cylinders`
	_sec_per_track=`expr $_sec_per_cyl / $_tracks_per_cyl`
	_new_c_size=`expr $_sec_per_track \* $_tracks_per_cyl \* $_cylinders`

	# Emit a disktab entry, suitable for getting started.
	# What we have is a `c' partition with the total number of
	# blocks, and an `a' partition with 1 sector; just large enough
	# to open.  Don't ask.
	echo	"" >> /etc/disktab
	echo	"# Created by install" >> /etc/disktab
	echo	"${_cur_disk_name}:\\" >> /etc/disktab
	echo -n	"	:ty=winchester:ns#${_sec_per_track}:" >> /etc/disktab
	echo	"nt#${_tracks_per_cyl}:nc#${_fudge_cyl}:\\" >> /etc/disktab
	echo	"	:pa#1:\\" >> /etc/disktab
	echo	"	:pc#${_nsectors}:" >> /etc/disktab

	# Ok, here's what we need to do.  First of all, we install
	# this initial label by opening the `c' partition of the disk
	# and using the `-r' flag for disklabel(8).  However, because
	# of limitations in disklabel(8), we've had to fudge the number
	# of cylinders up 1 so that disklabel(8) doesn't complain about
	# `c' running past the end of the disk, which can be quite
	# common even with OEM HP drives!  So, we've given ourselves
	# an `a' partition, which is the minimum needed to open the disk
	# so that we can perform the DIOCWDLABEL ioctl.  So, once the
	# initial label is installed, we open the `a' partition so that
	# we can fix up the number of cylinders and make the size of
	# `c' come out to (ncyl * ntracks_per_cyl * nsec_per_track).
	# After that's done, we re-open `c' and let the user actually
	# edit the partition table.  It's horrible, I know.  Bleh.

	disklabel -W ${1}
	if ! disklabel -r -w ${1} ${_cur_disk_name}; then
		echo ""
		echo "ERROR: can't bootstrap disklabel!"
		rval="1"
		return
	fi

	echo ""
	echo "The disklabel editor will now start.  During this phase, you"
	echo "must reset the 'cylinders' value to ${_cylinders}, and adjust"
	echo "the size of partition 'c' to ${_new_c_size}.  Do not modify"
	echo "the partition map at this time.  You will have the opportunity"
	echo "to do so in a moment."
	echo ""
	echo -n	"Press <return> to continue. "
	getresp ""

	disklabel -W ${1}
	if ! disklabel -r -e /dev/r${1}a; then
		echo ""
		echo "ERROR: can't fixup geometry!"
		rval="1"
		return
	fi

	cat << \__explain_motives_2

Now that you have corrected the geometry of your disk, you may edit the
partition map.  Don't forget to fill in the fsize (frag size), bsize
(filesystem block size), and cpg (cylinders per group) values.  If you
are unsure what these should be, use:

	fsize: 1024
	bsize: 4096
	cpg: 16

__explain_motives_2
	echo -n	"Press <return> to continue. "
	getresp ""

	rval="0"
	return
}

hp300_init_label_hpib_disk() {
	# $1 is the disk to label

	# We look though the boot messages attempting to find
	# the model number for the provided disk.
	_hpib_disktype=""
	if egrep "${1}: " < /kern/msgbuf > /dev/null 2>&1; then
		_hpib_disktype=HP`egrep "${1}: " < /kern/msgbuf | sort -u | \
		    awk '{print $2}'`
	fi
	if [ "X${_hpib_disktype}" = "X" ]; then
		echo ""
		echo "ERROR: $1 doesn't appear to exist?!"
		rval="1"
		return
	fi

	# Peer through /etc/disktab to see if the disk has a "default"
	# layout.  If it doesn't, we have to treat it like a SCSI disk;
	# i.e. prompt for geometry, and create a default to place
	# on the disk.
	if ! egrep "${_hpib_disktype}[:|]" /etc/disktab > /dev/null \
	    2>&1; then
		echo ""
		echo "WARNING: can't find defaults for $1 ($_hpib_disktype)"
		echo ""
		hp300_init_label_scsi_disk $1
		return
	fi

	# We've found the defaults.  Now use them to place an initial
	# disklabel on the disk.
	# XXX What kind of ugliness to we have to deal with to get around
	# XXX stupidity on the part of disklabel semantics?
	disklabel -W ${1}
	if ! disklabel -r -w ${1} $_hpib_disktype; then
		# Error message displayed by disklabel(8)
		echo ""
		echo "ERROR: can't install default label!"
		echo ""
		echo -n	"Try a different method? [y] "
		getresp "y"
		case "$resp" in
			y*|Y*)
				hp300_init_label_scsi_disk $1
				return
				;;

			*)
				rval="1"
				return
				;;
		esac
	fi

	rval="0"
	return
}

md_labeldisk() {
	# $1 is the disk to label

	# Check to see if there is a disklabel present on the device.
	# If so, we can just edit it.  If not, we must first install
	# a default label.
	md_checkfordisklabel $1
	case $? in
		0)
			# Go ahead and just edit the disklabel.
			disklabel -W $1
			disklabel -E $1
			;;

		*)
		echo -n "No disklabel present, installing a default for type: "
			case "$1" in
				hd*)
					echo "HP-IB"
					hp300_init_label_hpib_disk $1
					;;

				sd*)
					echo "SCSI"
					hp300_init_label_scsi_disk $1
					;;

				*)
					# Shouldn't happen, but...
					echo "unknown?!  Giving up."
					return;
					;;
			esac

			# Check to see if installing the default was
			# successful.  If so, go ahead and pop into the
			# disklabel editor.
			if [ "X${rval}" != X"0" ]; then
				echo "Sorry, can't label this disk."
				echo ""
				return;
			fi

			# We have some defaults installed.  Pop into
			# the disklabel editor.
			disklabel -W $1
			if ! disklabel -E $1; then
				echo ""
				echo "ERROR: couldn't set partition map for $1"
				echo ""
			fi
	esac
}

md_prep_disklabel()
{
	local _disk

	_disk=$1
	md_checkfordisklabel $_disk
	case $? in
	0)
		;;
	1)
		echo "WARNING: Disk $_disk has no label. You will be creating a new one."
		echo
		;;
	2)
		echo "WARNING: Label on disk $_disk is corrupted. You will be repairing."
		echo
		;;
	esac

	# display example
	cat << \__md_prep_disklabel_1

If you are unsure of how to use multiple partitions properly
(ie. seperating /, /usr, /tmp, /var, /usr/local, and other things)
just split the space into a root and swap partition for now.
__md_prep_disklabel_1

	disklabel -W ${_disk}
	disklabel -E ${_disk}
}

md_copy_kernel() {
	if [ ! -s /mnt/bsd ]; then
		echo    ""
		echo    "Warning, no kernel installed!"
		echo    "You did not unpack a file set containing a kernel."
		echo    "This is needed to boot.  Please note that the install"
		echo    "install kernel is not suitable for general use."
		echo -n "Escape to shell add /mnt/bsd by hand? [y] "
		getresp "y"
		case "$resp" in
			y*|Y*)
				echo "Type 'exit' to return to install."
				sh
				;;
			*)
				;;
		esac
	fi
}

# Note, while they might not seem machine-dependent, the
# welcome banner and the punt message may contain information
# and/or instructions specific to the type of machine.

md_welcome_banner() {
(
	if [ "$MODE" = "install" ]; then
		echo "Welcome to the OpenBSD/hp300 ${VERSION_MAJOR}.${VERSION_MINOR} installation program."
		cat << \__welcome_banner_1

This program is designed to help you put OpenBSD on your system in a
simple and rational way.
__welcome_banner_1

	else
		echo "Welcome to the OpenBSD/hp300 ${VERSION_MAJOR}.${VERSION_MINOR} upgrade program."
		cat << \__welcome_banner_2

This program is designed to help you upgrade your OpenBSD system in a
simple and rational way.

As a reminder, installing the `etc' binary set is NOT recommended.
Once the rest of your system has been upgraded, you should manually
merge any changes to files in the `etc' set into those files which
already exist on your system.

__welcome_banner_2
	fi

cat << \__welcome_banner_3

As with anything which modifies your disk's contents, this program can
cause SIGNIFICANT data loss, and you are advised to make sure your
data is backed up before beginning the installation process.

Default answers are displayed in brackets after the questions.  You
can hit Control-C at any time to quit, but if you do so at a prompt,
you may have to hit return.  Also, quitting in the middle of
installation may leave your system in an inconsistent state.

__welcome_banner_3
) | less -E
}

md_not_going_to_install() {
		cat << \__not_going_to_install_1

OK, then.  Enter 'halt' at the prompt to halt the machine.  Once the
machine has halted, power-cycle the system to load new boot code.

__not_going_to_install_1
}

md_congrats() {
	cat << \__congratulations_1

CONGRATULATIONS!  You have successfully installed OpenBSD!  To boot the
installed system, enter halt at the command prompt.  Once the system has
halted, power-cycle the machine in order to load new boot code.  Make sure
you boot from the root disk.

__congratulations_1
}

md_native_fstype() {
	# Nothing to do.
}

md_native_fsopts() {
	# Nothing to do.
}
