#!/bin/sh
#
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
MDSETS=""

md_set_term() {
	if [ ! -z "$TERM" ]; then
		return
	fi
	echo -n "Specify terminal type [hp300h]: "
	getresp "hp300h"
	TERM="$resp"
	export TERM
	# XXX call tset?
}

md_makerootwritable() {
	# Was: do_mfs_mount "/tmp" "2048"
	# /tmp is the mount point
	# 2048 is the size in DEV_BIZE blocks

	umount /tmp > /dev/null 2>&1
	if ! mount_mfs -s 2048 swap /tmp ; then
		cat << \__mfs_failed_1

FATAL ERROR: Can't mount the memory filesystem.

__mfs_failed_1
		exit
	fi

	# Bleh.  Give mount_mfs a chance to DTRT.
	sleep 2
}

md_get_diskdevs() {
	# return available disk devices
	dmesg | grep "^rd[0-9]*:." | cut -d":" -f1 | sort -u
	dmesg | grep "^sd[0-9]*:.*cylinders" | cut -d":" -f1 | sort -u
}

md_get_cddevs() {
	# return available CD-ROM devices
	dmesg | grep "sd[0-9]*:.*CD-ROM" | cut -d":" -f1 | sort -u
}

md_get_ifdevs() {
	# return available network interfaces
	dmesg | grep "^le[0-9]*:" | cut -d":" -f1 | sort -u
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
	more << \__scsi_label_1

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
			(echo ""; dmesg; echo "") | more
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
	if ! disklabel -w -r ${1} ${_cur_disk_name}; then
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
	if ! disklabel -e /dev/r${1}a; then
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
	if dmesg | grep "${1}: " > /dev/null 2>&1; then
		_hpib_disktype=HP`dmesg | grep "${1}: " | sort -u | \
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
	if ! grep "${_hpib_disktype}[:|]" /etc/disktab > /dev/null \
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
	case "$rval" in
		0)
			# Go ahead and just edit the disklabel.
			disklabel -W $1
			disklabel -e $1
			;;

		*)
		echo -n "No disklabel present, installing a default for type: "
			case "$1" in
				rd*)
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
			if ! disklabel -e $1; then
				echo ""
				echo "ERROR: couldn't set partition map for $1"
				echo ""
			fi
	esac
}

md_prep_disklabel() {
	# $1 is the root disk

	# Make sure there's a disklabel there.  If there isn't, puke after
	# disklabel prints the error message.
	md_checkfordisklabel $1
	case "$resp" in
		1)
			cat << \__md_prep_disklabel_1

FATAL ERROR: There is no disklabel present on the root disk!  You must
label the disk with SYS_INST before continuing.

__md_prep_disklabel_1
			exit
			;;

		2)
			cat << \__md_prep_disklabel_2

FATAL ERROR: The disklabel on the root disk is corrupted!  You must
re-label the disk with SYS_INST before continuing.

__md_prep_disklabel_2
			exit
			;;

		*)
			;;
	esac

	# Give the user the opportinuty to edit the root disklabel.
	cat << \__md_prep_disklabel_3

You have already placed a disklabel onto the target root disk.
However, due to the limitations of the standalone program used
you may want to edit that label to change partition type information.
You will be given the opporunity to do that now.  Note that you may
not change the size or location of any presently open partition.

__md_prep_disklabel_3
	echo -n "Do you wish to edit the root disklabel? [y] "
	getresp "y"
	case "$resp" in
		y*|Y*)
			disklabel -W $1
			disklabel -e $1
			;;

		*)
			;;
	esac

	cat << \__md_prep_disklabel_4

You will now be given the opportunity to place disklabels on any additional
disks on your system.
__md_prep_disklabel_4

	_DKDEVS=`rmel ${ROOTDISK} ${_DKDEVS}`
	resp="X"	# force at least one iteration
	while [ "X$resp" != X"done" ]; do
		labelmoredisks
	done
}

md_copy_kernel() {
	echo -n "Copying kernel..."
	cp -p /netbsd /mnt/netbsd
	echo "done."
}

	# Note, while they might not seem machine-dependent, the
	# welcome banner and the punt message may contain information
	# and/or instructions specific to the type of machine.

md_welcome_banner() {
(
	echo	""
	echo	"Welcome to the NetBSD/hp300 ${VERSION} installation program."
	cat << \__welcome_banner_1

This program is designed to help you install NetBSD on your system in a
simple and rational way.  You'll be asked several questions, and it would
probably be useful to have your disk's hardware manual, the installation
notes, and a calculator handy.

In particular, you will need to know some reasonably detailed
information about your disk's geometry.  This program can determine
some limited information about certain specific types of HP-IB disks.
If you have SCSI disks, however, prior knowledge of disk geometry
is absolutely essential.  The kernel will attempt to display geometry
information for SCSI disks during boot, if possible.  If you did not
make it note of it before, you may wish to reboot and jot down your
disk's geometry before proceeding.

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

CONGRATULATIONS!  You have successfully installed NetBSD!  To boot the
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
