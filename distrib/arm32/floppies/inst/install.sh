#!/bin/sh
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
#	$Id: install.sh,v 1.3 1998/11/03 04:17:19 aaron Exp $

#	NetBSD installation script.
#	In a perfect world, this would be a nice C program, with a reasonable
#	user interface.

stty erase ^H

DT=/etc/disktab				# /etc/disktab
FSTABDIR=/mnt/etc			# /mnt/etc
#DONTDOIT=echo

VERSION=1.1
FSTAB=${FSTABDIR}/fstab

getresp() {
	read resp
	if [ "X$resp" = "X" ]; then
		resp=$1
	fi
}

echo	"Welcome to the NetBSD ${VERSION} installation program."
echo	""
echo	"This program is designed to help you put NetBSD on your hard disk,"
echo	"in a simple and rational way.  You'll be asked several questions,"
echo	"and it would probably be useful to have your disk's hardware"
echo	"manual, the installation notes, and a calculator handy."
echo	""
echo	"In particular, you will need to know some reasonably detailed"
echo	"information about your disk's geometry, because there is currently"
echo	"no way this this program can figure that information out."
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
		echo	"Cool!  Let's get to it..."
		;;
	*)
		echo	""
		echo	"OK, then.  Enter 'halt' at the prompt to halt the"
		echo	"machine."
		exit
		;;
esac

echo	""
echo	"To do the installation, you'll need to provide some information about"
echo	"your disk."
echo	""
echo	"NetBSD can be installed on ST506, ESDI, IDE, or SCSI disks."
echo -n	"What kind of disk will you be installing on? [IDE] "
getresp "IDE"
case "$resp" in
	esdi|ESDI|st506|ST506)
		drivetype=wd
		echo -n "Does it support _automatic_ sector remapping? [y] "
		getresp "y"
		case "$resp" in
			n*|N*)
				sect_fwd="sf:"
				;;
			*)
				sect_fwd=""
				;;
		esac
		;;
	ide|IDE)
		drivetype=wd
		sect_fwd=""
		type=ST506
		;;
	scsi|SCSI)
		drivetype=sd
		sect_fwd=""
		type=SCSI
		;;
esac

# find out what units are possible for that disk, and query the user.
driveunits=`ls /dev/${drivetype}?a | sed -e 's,/dev/\(...\)a,\1,g'`
if [ "X${driveunits}" = "X" ]; then
	echo	"FATAL ERROR:"
	echo	"No devices for disks of type '${drivetype}'."
	echo	"This is probably a bug in the install disks."
	echo	"Exiting install program."
	exit
fi
prefdrive=${drivetype}0

echo	""
echo	"The following ${drivetype}-type disks are supported by this"
echo	"installation procedure:"
echo	"	"${driveunits}
echo	"Note that they may not exist in _your_ machine; the list of"
echo	"disks in your machine was printed when the system was booting."
echo	""
while [ "X${drivename}" = "X" ]; do
	echo -n	"Which disk would like to install on? [${prefdrive}] "
	getresp ${prefdrive}
	otherdrives=`echo "${driveunits}" | sed -e s,${resp},,`
	if [ "X${driveunits}" = "X${otherdrives}" ]; then
		echo	""
		echo	"\"${resp}\" is an invalid drive name.  Valid choices"
		echo	"are: "${driveunits}
		echo	""
	else
		drivename=${resp}
	fi
done

echo	""
echo	"Using disk ${drivename}."
echo	""
echo -n	"What kind of disk is it? (one word please) [my${drivetype}] "
getresp "my${drivetype}"
labelname=$resp

echo	""
echo	"You will now need to provide some information about your disk's"
echo	"geometry.  This should either be in the User's Manual for your disk,"
echo	"or you should have written down what NetBSD printed when booting."
echo	"(Note that he geometry that's printed at boot time is preferred.)"
echo	""
echo    "You may choose to view the initial boot messages for your system"
echo    "again right now if you like."
echo -n "View the boot messages again? [n] "
getresp "n"
case "$resp" in
	y*|Y*)
		more /kern/msgbuf
		;;
	*)
		echo	""
		;;
esac

echo	""
echo	"You will now enter the disk geometry information"
echo	""

echo -n	"Number of bytes per disk sector? [512] "
getresp 512
bytes_per_sect="$resp"

echo -n "Number of disk cylinders? "
getresp
cyls_per_disk="$resp"

echo -n	"Number of disk tracks (heads) per disk cylinder? "
getresp
tracks_per_cyl="$resp"

echo -n	"Number of disk sectors per disk track? "
getresp
sects_per_track="$resp"

cylindersize=`expr $sects_per_track \* $tracks_per_cyl`
cylbytes=`expr $cylindersize \* $bytes_per_sect`
disksize=`expr $cylindersize \* $cyls_per_disk`

echo	""
echo	"Your disk has a total of $disksize $bytes_per_sect byte sectors,"
echo	"arranged as $cyls_per_disk cylinders which contain $cylindersize "
echo	"sectors ($cylbytes bytes) each."
echo	""
echo	"You can specify partition sizes in cylinders ('c') or sectors ('s')."
while [ "X${sizemult}" = "X" ]; do
	echo -n	"What units would you like to use? [cylinders] "
	getresp cylinders
	case "$resp" in
		c*|C*)
			sizemult=$cylindersize
			sizeunit="cylinders"
			;;
		s*|S*)
			sizemult=1
			sizeunit="sectors"
			;;
		*)
			echo	""
			echo	"Enter cylinders ('c') or sectors ('s')."
			;;
	esac
done

if [ $sizeunit = "sectors" ]; then
	echo	""
	echo	"For best disk performance, partitions should begin and end on"
	echo	"cylinder boundaries.  Wherever possible, pick sizes that are"
	echo	"multiples of the cylinder size ($cylindersize sectors)."
fi

echo -n ""
echo -n "Size of NetBSD portion of disk (in $sizeunit)? "
getresp
partition=$resp
partition_sects=`expr $resp \* $sizemult`
part_offset=0
if [ $partition_sects -lt $disksize ]; then
	echo -n "Offset of NetBSD portion of disk (in $sizeunit)? "
	getresp
	part_offset=$resp
fi
badspacesec=0
if [ "$sect_fwd" = "sf:" ]; then
	badspacecyl=`expr $sects_per_track + 126`
	badspacecyl=`expr $badspacecyl + $cylindersize - 1`
	badspacecyl=`expr $badspacecyl / $cylindersize`
	badspacesec=`expr $badspacecyl \* $cylindersize`
	echo	""
	echo -n "Using $badspacesec sectors ($badspacecyl cylinders) for the "
	echo	"bad144 bad block table"
fi

sects_left=`expr $partition_sects - $badspacesec`
units_left=`expr $sects_left / $sizemult`
echo	""
echo	"There are $units_left $sizeunit left to allocate."
echo	""
root=0
while [ $root -eq 0 ]; do
	echo -n "Root partition size (in $sizeunit)? "
	getresp
	case $resp in
		[1-9]*)
			total=$resp
			if [ $total -gt $units_left ]; then
				echo -n	"Root size is greater than remaining "
				echo	"free space on disk."
			else
				root=$resp
			fi
			;;
	esac
done
root_offset=$part_offset
part_used=`expr $root + $badspacesec / $sizemult`
units_left=`expr $partition - $part_used`
echo	""

swap=0
while [ $swap -eq 0 ]; do 
	echo	"$units_left $sizeunit remaining in NetBSD portion of disk."
	echo -n	"Swap partition size (in $sizeunit)? "
	getresp
	case $resp in
		[1-9]*)
			if [ $swap -gt $units_left ]; then
				echo -n	"Swap size is greater than remaining "
				echo	"free space on disk."
			else
				swap=$resp
			fi
			;;
	esac
done
swap_offset=`expr $root_offset + $root`
part_used=`expr $part_used + $swap`
echo	""

fragsize=1024
blocksize=8192
$DONTDOIT mount -u /dev/rd0 /
cat /etc/disktab.preinstall > $DT
echo	"" >> $DT
echo	"$labelname|NetBSD installation generated:\\" >> $DT
echo	"	:dt=${type}:ty=winchester:\\" >> $DT
echo -n	"	:nc#${cyls_per_disk}:ns#${sects_per_track}" >> $DT
echo	":nt#${tracks_per_cyl}:\\" >> $DT
echo	"	:se#${bytes_per_sect}:${sect_fwd}\\" >> $DT
_size=`expr $root \* $sizemult`
_offset=`expr $root_offset \* $sizemult`
echo -n	"	:pa#${_size}:oa#${_offset}" >> $DT
echo	":ta=4.2BSD:ba#${blocksize}:fa#${fragsize}:\\" >> $DT
_size=`expr $swap \* $sizemult`
_offset=`expr $swap_offset \* $sizemult`
echo	"	:pb#${_size}:ob#${_offset}:tb=swap:\\" >> $DT
_size=`expr $partition \* $sizemult`
echo	"	:pd#${_size}:od#0:\\" >> $DT

echo	"You will now have to enter information about any other partitions"
echo	"to be created in the NetBSD portion of the disk.  This process will"
echo	"be complete when you've filled up all remaining space in the NetBSD"
echo	"portion of the disk."

while [ $part_used -lt $partition ]; do
	part_size=0
	units_left=`expr $partition - $part_used`
	while [ $part_size -eq 0 ]; do
		echo	""
		echo -n	"$units_left $sizeunit remaining in NetBSD portion of "
		echo	"the disk"
		echo -n "Next partition size (in $sizeunit)? "
		getresp
		case $resp in
			[1-9]*)
				total=`expr $part_used + $resp`
				if [ $total -gt $partition ]; then
					echo -n	"That would make the partition"
					echo	"too large to fit!"
				else
					part_size=$resp
					part_used=$total
					part_name=""
					while [ "$part_name" = "" ]; do
						echo -n "Mount point? "
						getresp
						part_name=$resp
					done
				fi
				;;
		esac
	done
	if [ "$ename" = "" ]; then
		ename=$part_name
		offset=`expr $part_offset + $root + $swap`
		_size=`expr $part_size \* $sizemult`
		_offset=`expr $offset \* $sizemult`
		echo -n "	:pe#${_size}:oe#${_offset}" >> $DT
		echo ":te=4.2BSD:be#${blocksize}:fe#${fragsize}:\\" >> $DT
		offset=`expr $offset + $part_size`
	elif [ "$fname" = "" ]; then
		fname=$part_name
		_size=`expr $part_size \* $sizemult`
		_offset=`expr $offset \* $sizemult`
		echo -n "	:pf#${_size}:of#${_offset}" >> $DT
		echo ":tf=4.2BSD:bf#${blocksize}:ff#${fragsize}:\\" >> $DT
		offset=`expr $offset + $part_size`
	elif [ "$gname" = "" ]; then
		gname=$part_name
		_size=`expr $part_size \* $sizemult`
		_offset=`expr $offset \* $sizemult`
		echo -n "	:pg#${_size}:og#${_offset}" >> $DT
		echo ":tg=4.2BSD:bg#${blocksize}:fg#${fragsize}:\\" >> $DT
		offset=`expr $offset + $part_size`
	elif [ "$hname" = "" ]; then
		hname=$part_name
		_size=`expr $part_size \* $sizemult`
		_offset=`expr $offset \* $sizemult`
		echo -n "	:ph#${_size}:oh#${_offset}" >> $DT
		echo ":th=4.2BSD:bh#${blocksize}:fh#${fragsize}:\\" >> $DT
		part_used=$partition
	fi
done
echo	"	:pc#${disksize}:oc#0:" >> $DT
sync

echo	""
echo	"THIS IS YOUR LAST CHANCE!!!"
echo	""
echo -n	"Are you SURE you want NetBSD installed on your hard drive? (yes/no) "
answer=""
while [ "$answer" = "" ]; do
	getresp
	case $resp in
		yes|YES)
			echo	""
			echo	"Here we go..."
			answer=yes
			;;
		no|NO)
			echo	""
			echo -n	"OK, then.  enter 'halt' to halt the machine.  "
			exit
			;;
		*)
			echo -n "I want a yes or no answer...  well? "
			;;
	esac
done

echo	""
echo -n	"Labeling disk $drivename..."
$DONTDOIT disklabel -w -r $drivename $labelname
echo	" done."

if [ "$sect_fwd" = "sf:" ]; then
	echo -n "Initializing bad144 badblock table..."
	$DONTDOIT bad144 $drivename 0
	echo " done."
fi

echo	"Initializing root filesystem, and mounting..."
$DONTDOIT newfs /dev/r${drivename}a
$DONTDOIT mount -v /dev/${drivename}a /mnt
if [ "$ename" != "" ]; then
	echo	""
	echo	"Initializing $ename filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}e
	$DONTDOIT mkdir -p /mnt/$ename
	$DONTDOIT mount -v /dev/${drivename}e /mnt/$ename
fi
if [ "$fname" != "" ]; then
	echo	""
	echo	"Initializing $fname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}f
	$DONTDOIT mkdir -p /mnt/$fname
	$DONTDOIT mount -v /dev/${drivename}f /mnt/$fname
fi
if [ "$gname" != "" ]; then
	echo	""
	echo	"Initializing $gname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}g
	$DONTDOIT mkdir -p /mnt/$gname
	$DONTDOIT mount -v /dev/${drivename}g /mnt/$gname
fi
if [ "$hname" != "" ]; then
	echo	""
	echo	"Initializing $hname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}h
	$DONTDOIT mkdir -p /mnt/$hname
	$DONTDOIT mount -v /dev/${drivename}h /mnt/$hname
fi

echo	""
echo    "Populating filesystems with bootstrapping binaries and config files"
$DONTDOIT tar --one-file-system -cf - . | (cd /mnt ; tar --unlink -xpf - )
$DONTDOIT cp /tmp/.hdprofile /mnt/.profile

echo	""
echo -n	"Creating an fstab..."
echo /dev/${drivename}a / ffs rw 1 1 | sed -e s,//,/, > $FSTAB
if [ "$ename" != "" ]; then
	echo /dev/${drivename}e /$ename ffs rw 1 2 | sed -e s,//,/, >> $FSTAB
fi
if [ "$fname" != "" ]; then
	echo /dev/${drivename}f /$fname ffs rw 1 3 | sed -e s,//,/, >> $FSTAB
fi
if [ "$gname" != "" ]; then
	echo /dev/${drivename}g /$gname ffs rw 1 4 | sed -e s,//,/, >> $FSTAB
fi
if [ "$hname" != "" ]; then
	echo /dev/${drivename}h /$hname ffs rw 1 5 | sed -e s,//,/, >> $FSTAB
fi
sync
echo	" done."

echo	""
echo	""
echo	"OK!  The preliminary work of setting up your disk is now complete."
echo 	""
echo	"The remaining tasks are:"
echo	""
echo	"To copy a NetBSD kernel to the hard drive's root filesystem."
echo	"Once accomplished, you can boot off the hard drive."
echo	""
echo	"To load and install the NetBSD distribution sets."
echo	"Currently the hard drive's root filesystem is mounted on /mnt"
echo	""
echo	"Consult the installation notes which will describe how to"
echo	"install the distribution sets and kernel.  Post-installation"
echo	"configuration is also discussed therein."
echo	""
echo	"GOOD LUCK!"
echo	""
