#!/bin/sh
#
#	$OpenBSD: install.sh,v 1.6 2000/03/01 22:10:01 todd Exp $
#	$NetBSD: install.sh,v 1.2 1995/08/25 19:17:28 leo Exp $
#
# Copyright (c) 1994 Christopher G. Demetriou
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
#	This product includes software developed by Christopher G. Demetriou.
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

FSTABDIR=/mnt/etc			# /mnt/etc
#DONTDOIT=echo

VERSION=1.0
FSTAB=${FSTABDIR}/fstab

getresp() {
	read resp
	if [ "X$resp" = "X" ]; then
		resp=$1
	fi
}

getvar() {
	echo $(eval $(echo "echo \$$1"))
}

shiftvar() {
	local - var
	var="$1"
	list="$(getvar $var)"
	set -- $list
	shift
	setvar $var "$*"
}

getparts() {
	disklabel $1 2>/dev/null | sed -e '/^[ ][ ][ad-p]/!d' |
	sed -e 's,^[ ]*\([a-p]\):[ ]*[0-9]*[ ]*[0-9]*[ ][ ]*\([a-zA-Z0-9.]*\).*,\1 \2,' |
	sed -e ':a
		N;${s/\n/ /g;p;d;}
		ba'
}

getdrives() {
	local du thispart
	for du in /dev/r${drivetype}?a; do
		dd if=$du of=/dev/null bs=1b count=1 >/dev/null 2>&1
		if [ $? -eq 0 ]; then
			thisunit=`echo $du | sed -e 's,/dev/r\(...\)a,\1,g'`
			driveunits="$driveunits $thisunit"
		else
			continue;
		fi
		setvar $thisunit "$(getparts $thisunit)"
		export $thisunit
	done
	export drivenunits
}

prepdrive() {
	echo	"which drive would you like to prepare next?"
	echo	"choices are: ${driveunits}"
	echo	""
	getresp
	case $resp in
	*)	;;
	esac
}

echo	"Welcome to the NetBSD ${VERSION} installation program."
echo	""
echo	"This program is designed to help you put NetBSD on your hard disk,"
echo	"in a simple and rational way.  Its main objective is to format,"
echo	"mount and create an fstab for your root (/) and user (/usr)"
echo	"partitions."
echo	""
echo	"As with anything which modifies your hard drive's contents, this"
echo	"program can cause SIGNIFICANT data loss, and you are advised"
echo	"to make sure your hard drive is backed up before beginning the"
echo	"installation process."
echo	""
echo	"Default answers are displyed in brackets after the questions."
echo	"You can hit Control-C at any time to quit, but if you do so at a"
echo	"prompt, you may have to hit return.  Also, quitting in the middle of"
echo	"installation may leave your system in an inconsistent state."
echo	""
echo -n "Proceed with installation? [n] "
getresp "n"
case "$resp" in
	y*|Y*)
		echo	"scanning for the root device"
		;;
	*)
		echo	""
		echo	"OK, then.  Enter 'halt' at the prompt to halt the"
		echo	"machine.  Once the machine has halted, remove the"
		echo	"floppy and press any key to reboot."
		exit
		;;
esac

drivetype=sd
sect_fwd=""

# find out what units are possible for that disk, and query the user.
getdrives
for du in $driveunits; do
	set -- $(getvar $du)
	if [ $# -ge 2 -a "$1" = "a" -a "`echo $2 | sed -e 's,.*BSD.*,BSD,'`" = "BSD" ]; then
		rdev=$du
	fi
done

echo	""
echo	"The root device you have chosen is on: ${rdev}"
echo	""
# driveunits=`ls /dev/${drivetype}?a | sed -e 's,/dev/\(...\)a,\1,g'`
if [ "X${driveunits}" = "X" ]; then
	echo	"FATAL ERROR:"
	echo	"No devices for disks of type '${drivetype}'."
	echo	"This is probably a bug in the install disks."
	echo	"Exiting install program."
	exit
fi

echo	""
echo	"THIS IS YOUR LAST CHANCE!!!"
echo	""
echo	"(answering yes will format your root partition on $rdev)"
echo -n	"Are you SURE you want NetBSD installed on your hard drive? (yes/no) "
answer=""
while [ "$answer" = "" ]; do
	getresp
	case $resp in
		yes|YES)
			echo	""
			answer=yes
			;;
		no|NO)
			echo	""
			echo -n	"OK, then.  enter 'halt' to halt the machine.  "
			echo    "Once the machine has halted,"
			echo -n	"remove the floppy, and press any key to "
			echo	"reboot."
			exit
			;;
		*)
			echo -n "I want a yes or no answer...  well? "
			;;
	esac
done
echo	"Initializing / (root) filesystem, and mounting..."
$DONTDOIT newfs /dev/r${rdev}a $name
$DONTDOIT mount_ffs /dev/${rdev}a /mnt
echo	""
echo -n	"Creating a fstab..."
mkdir -p $FSTABDIR
echo "/dev/${rdev}a	/	ffs	rw	1	1" > $FSTAB

# get rid of this partition
shiftvar $rdev
shiftvar $rdev

echo	""
echo	"Now lets setup your /usr file system"
echo	"(Once a valid input for drive and partition is seen"
echo	"it will be FORMATTED and inserted in the fstab.)"
while [ "X$usrpart" = "X" ]; do
	resp=""
	drivename=""
	while [ "X$resp" = "X" ]; do
		echo	"choices: $driveunits"
		echo	"which drive do you want /usr on?"
		getresp
		set -- $driveunits
		while [ $# -gt 0 ]; do
			if [ "X$resp" = "X$1" ]; then
				drivename=$1
				break;
			else
				shift
			fi
		done
		if [ "X$drivename" != "X" ]; then
			break
		fi
	done

	usrpart=""
	echo	"You have selected $drivename"
	echo	"here is a list of partitions on $drivename"
	disklabel $drivename 2>/dev/null | sed -e '/^[ ][ ][ad-p]:/p;/^#[ \t]*size/p;d' 
	echo	"which partition would you like to format and have"
	echo -n	"mounted as /usr? (supply the letter): "
	getresp
	if [ "X$resp" = "X" ]; then
		continue;
	fi

	list=$(getvar $drivename)
	set -- $list
	while [ $# -gt 0 ]; do
		if [ "$resp" = "$1" ]; then
			if [ "`echo $2 | sed -e 's,.*BSD.*,BSD,'`" != "BSD" ]; then
				echo	""
				echo -n	"$drivename$resp is of type $2 which is not"
				echo	" a BSD filesystem type"
				break
			fi
			usrpart=$drivename$resp
			break
		else
			shift
			shift
		fi
	done
	if [ "X$usrpart" = "X" ]; then
		echo	"$resp is not a valid input."
		echo	""
	fi
done

echo	""
echo	"Initializing /usr filesystem, and mounting..."
$DONTDOIT newfs /dev/r${usrpart} $name
$DONTDOIT mkdir -p /mnt/usr
$DONTDOIT mount_ffs /dev/${usrpart} /mnt/usr
echo	""
echo -n	"Adding to fstab..."
echo "/dev/${usrpart}	/usr	ffs	rw	1	2" >> $FSTAB
sync
echo	" done."

echo	""
echo	""
echo	"OK!  The preliminary work of setting up your disk is now complete,"
echo	"and you can install the actual NetBSD software."
echo	""
echo	"Right now, your root is mounted on /mnt and your usr on /mnt/usr."
echo	"You should consult the installation notes to determine how to load"
echo	"and install the NetBSD distribution sets, and how to configure your"
echo	"system when you are done."
echo	""
echo	"GOOD LUCK!"
echo	""
