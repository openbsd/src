#!/bin/sh
#	$OpenBSD: install.sh,v 1.20 1998/11/03 04:17:20 aaron Exp $
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

#	OpenBSD installation script.
#	In a perfect world, this would be a nice C program, with a reasonable
#	user interface.

DT=/etc/disktab				# /etc/disktab
FSTABDIR=/mnt/etc			# /mnt/etc
#DONTDOIT=echo

VERSION=2.0
FSTAB=${FSTABDIR}/fstab

getresp() {
	read resp
	if [ "X$resp" = "X" ]; then
		resp=$1
	fi
}

echo	"Welcome to the OpenBSD ${VERSION} installation program."
echo	""
echo	"This program is will put OpenBSD on your hard disk.  It is not"
echo	"painless, but it could be worse.  You'll be asked several questions,"
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
	echo	"OK, then.  Enter 'halt' at the prompt to halt the"
	echo	"machine.  Once the machine has halted, remove the"
	echo	"floppy and press any key to reboot."
	exit
	;;
esac

echo	""
echo	"To do the installation, you'll need to provide some information about"
echo	"your disk."

echo	"OpenBSD can be installed on ST506, ESDI, IDE, or SCSI disks."
echo -n	"What kind of disk will you be installing on? [SCSI] "
getresp "SCSI"
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

echo	"The following ${drivetype}-type disks are supported by this"
echo	"installation procedure:"
echo	"${driveunits}"
echo	"Note that they may not exist in _your_ machine; the list of"
echo	"disks in your machine was printed when the system was booting."
while [ "X${drivename}" = "X" ]; do
	echo -n	"Which disk would like to install on? [${prefdrive}] "
	getresp ${prefdrive}
	otherdrives=`echo "${driveunits}" | sed -e s,${resp},,`
	if [ "X${driveunits}" = "X${otherdrives}" ]; then
		echo	"\"${resp}\" is an invalid drive name.  Valid choices"
		echo	"are: "${driveunits}
	else
		drivename=${resp}
	fi
done

echo	""
echo	"Using disk ${drivename}."
echo -n	"What kind of disk is it? (one word please) [my${drivetype}] "
getresp "my${drivetype}"
labelname=$resp

echo	""
echo	"You will now need to provide some information about your disk's"
echo	"geometry.  This should either be in the User's Manual for your disk,"
echo	"or you should have written down what OpenBSD printed when booting."
echo	"(Note that the geometry that's printed at boot time is preferred.)"
echo	""
echo    "You may choose to view the initial boot messages for your system"
echo    "again right now if you like."
echo -n "View the boot messages again? [n] "
getresp "n"
case "$resp" in
y*|Y*)
	less -rsS /kern/msgbuf
	;;
*)
	echo	""
	;;
esac

echo	"You will now enter the disk geometry information"
echo	""

bytes_per_sect=`cat /kern/msgbuf \
	         | sed -n -e /^${drivename}:/p -e /^${drivename}:/q \
	         | awk '{ print $9 }'`
echo -n	"Number of bytes per disk sector? [$bytes_per_sect] "
getresp $bytes_per_sect
bytes_per_sect="$resp"

cyls_per_disk=`cat /kern/msgbuf \
	       | sed -n -e /^${drivename}:/p -e /^${drivename}:/q \
	       | awk '{ print $3 }'`
echo -n "Number of disk cylinders? [$cyls_per_disk]"
getresp $cyls_per_disk
cyls_per_disk="$resp"

tracks_per_cyl=`cat /kern/msgbuf \
	        | sed -n -e /^${drivename}:/p -e /^${drivename}:/q \
	        | awk '{ print $5 }'`
echo -n	"Number of disk tracks (heads) per disk cylinder? [$tracks_per_cyl]"
getresp $tracks_per_cyl
tracks_per_cyl="$resp"

sects_per_track=`cat /kern/msgbuf \
	         | sed -n -e /^${drivename}:/p -e /^${drivename}:/q \
	         | awk '{ print $7 }'`
echo -n	"Number of disk sectors per disk track? [$sects_per_track]"
getresp $sects_per_track
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
		maxdisk=$cyls_per_disk
		;;
	s*|S*)
		sizemult=1
		sizeunit="sectors"
		maxdisk=$disksize;
		;;
	*)
		echo	"Enter cylinders ('c') or sectors ('s')."
		;;
	esac
done

if [ $sizeunit = "sectors" ]; then
	echo "For best disk performance or workable CHS-translating IDE systems,"
	echo "partitions should begin and end on cylinder boundaries.  Wherever"
	echo "possible, use multiples of the cylinder size ($cylindersize sectors)."
fi

echo -n ""
echo -n "Size of OpenBSD portion of disk (in $sizeunit) ? [$maxdisk] "
getresp "$maxdisk"
partition=$resp
partition_sects=`expr $resp \* $sizemult`
part_offset=0
if [ $partition_sects -lt $disksize ]; then
	echo -n "Offset of OpenBSD portion of disk (in $sizeunit)? "
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
	echo	"$units_left $sizeunit remaining in OpenBSD portion of disk."
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
$DONTDOIT fsck -t ffs /dev/rfd0a
$DONTDOIT mount -u /dev/fd0a /
cat /etc/disktab.preinstall > $DT
echo	"" >> $DT
echo	"$labelname|OpenBSD installation generated:\\" >> $DT
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
echo	"	:pc#${disksize}:oc#0:\\" >> $DT

echo	"You will now have to enter information about any other partitions"
echo	"to be created in the OpenBSD portion of the disk.  This process will"
echo	"be complete when you've filled up all remaining space in the OpenBSD"
echo	"portion of the disk."

while [ $part_used -lt $partition ]; do
	part_size=0
	units_left=`expr $partition - $part_used`
	while [ $part_size -eq 0 ]; do
		echo	""
		echo -n	"$units_left $sizeunit remaining in OpenBSD portion of "
		echo	"the disk"
		echo -n "Next partition size (in $sizeunit) [$units_left] ? "
		getresp "$units_left"
		case $resp in
		[1-9]*)
			total=`expr $part_used + $resp`
			if [ $total -gt $partition ]; then
				echo "That would make the partition too large to fit!"
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
	# XXX we skip partition d to avoid user confusion
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
	elif [ "$iname" = "" ]; then
		iname=$part_name
		_size=`expr $part_size \* $sizemult`
		_offset=`expr $offset \* $sizemult`
		echo -n "	:pi#${_size}:oi#${_offset}" >> $DT
		echo ":ti=4.2BSD:bi#${blocksize}:fi#${fragsize}:\\" >> $DT
		part_used=$partition
	elif [ "$jname" = "" ]; then
		jname=$part_name
		_size=`expr $part_size \* $sizemult`
		_offset=`expr $offset \* $sizemult`
		echo -n "	:pj#${_size}:oj#${_offset}" >> $DT
		echo ":tj=4.2BSD:bj#${blocksize}:fj#${fragsize}:\\" >> $DT
		part_used=$partition
	elif [ "$kname" = "" ]; then
		kname=$part_name
		_size=`expr $part_size \* $sizemult`
		_offset=`expr $offset \* $sizemult`
		echo -n "	:pk#${_size}:ok#${_offset}" >> $DT
		echo ":tk=4.2BSD:bk#${blocksize}:fk#${fragsize}:\\" >> $DT
		part_used=$partition
	elif [ "$lname" = "" ]; then
		lname=$part_name
		_size=`expr $part_size \* $sizemult`
		_offset=`expr $offset \* $sizemult`
		echo -n "	:pl#${_size}:ol#${_offset}" >> $DT
		echo ":tl=4.2BSD:bl#${blocksize}:fl#${fragsize}:\\" >> $DT
		part_used=$partition
	elif [ "$mname" = "" ]; then
		mname=$part_name
		_size=`expr $part_size \* $sizemult`
		_offset=`expr $offset \* $sizemult`
		echo -n "	:pm#${_size}:om#${_offset}" >> $DT
		echo ":tm=4.2BSD:bm#${blocksize}:fm#${fragsize}:\\" >> $DT
		part_used=$partition
	elif [ "$nname" = "" ]; then
		nname=$part_name
		_size=`expr $part_size \* $sizemult`
		_offset=`expr $offset \* $sizemult`
		echo -n "	:pn#${_size}:on#${_offset}" >> $DT
		echo ":tn=4.2BSD:bn#${blocksize}:fn#${fragsize}:\\" >> $DT
		part_used=$partition
	elif [ "$oname" = "" ]; then
		oname=$part_name
		_size=`expr $part_size \* $sizemult`
		_offset=`expr $offset \* $sizemult`
		echo -n "	:po#${_size}:oo#${_offset}" >> $DT
		echo ":to=4.2BSD:bo#${blocksize}:fo#${fragsize}:\\" >> $DT
		part_used=$partition
	elif [ "$pname" = "" ]; then
		pname=$part_name
		_size=`expr $part_size \* $sizemult`
		_offset=`expr $offset \* $sizemult`
		echo -n "	:pp#${_size}:op#${_offset}" >> $DT
		echo ":tp=4.2BSD:bp#${blocksize}:fp#${fragsize}:\\" >> $DT
		part_used=$partition
	fi
done
echo "" >> $DT
sync

echo	""
echo	"THIS IS YOUR LAST CHANCE!!!"
echo	""
echo -n	"Are you SURE you want OpenBSD installed on your hard drive? (yes/no) "
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

echo	""
echo -n	"Labeling disk $drivename..."
$DONTDOIT disklabel -w -B $drivename $labelname
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
if [ "$iname" != "" ]; then
	echo	""
	echo	"Initializing $iname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}i
	$DONTDOIT mkdir -p /mnt/$iname
	$DONTDOIT mount -v /dev/${drivename}i /mnt/$iname
fi
if [ "$jname" != "" ]; then
	echo	""
	echo	"Initializing $jname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}j
	$DONTDOIT mkdir -p /mnt/$jname
	$DONTDOIT mount -v /dev/${drivename}j /mnt/$jname
fi
if [ "$kname" != "" ]; then
	echo	""
	echo	"Initializing $kname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}k
	$DONTDOIT mkdir -p /mnt/$kname
	$DONTDOIT mount -v /dev/${drivename}k /mnt/$kname
fi
if [ "$lname" != "" ]; then
	echo	""
	echo	"Initializing $lname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}l
	$DONTDOIT mkdir -p /mnt/$lname
	$DONTDOIT mount -v /dev/${drivename}l /mnt/$lname
fi
if [ "$mname" != "" ]; then
	echo	""
	echo	"Initializing $mname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}m
	$DONTDOIT mkdir -p /mnt/$mname
	$DONTDOIT mount -v /dev/${drivename}m /mnt/$mname
fi
if [ "$nname" != "" ]; then
	echo	""
	echo	"Initializing $nname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}n
	$DONTDOIT mkdir -p /mnt/$nname
	$DONTDOIT mount -v /dev/${drivename}n /mnt/$nname
fi
if [ "$oname" != "" ]; then
	echo	""
	echo	"Initializing $oname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}o
	$DONTDOIT mkdir -p /mnt/$oname
	$DONTDOIT mount -v /dev/${drivename}o /mnt/$oname
fi
if [ "$pname" != "" ]; then
	echo	""
	echo	"Initializing $pname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}p
	$DONTDOIT mkdir -p /mnt/$pname
	$DONTDOIT mount -v /dev/${drivename}p /mnt/$pname
fi

echo	""
echo    "Populating filesystems with bootstrapping binaries and config files"
$DONTDOIT tar -cXf - . | (cd /mnt ; tar -xpf - )
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
if [ "$iname" != "" ]; then
	echo /dev/${drivename}i /$iname ffs rw 1 5 | sed -e s,//,/, >> $FSTAB
fi
if [ "$jname" != "" ]; then
	echo /dev/${drivename}j /$jname ffs rw 1 5 | sed -e s,//,/, >> $FSTAB
fi
if [ "$kname" != "" ]; then
	echo /dev/${drivename}k /$kname ffs rw 1 5 | sed -e s,//,/, >> $FSTAB
fi
if [ "$lname" != "" ]; then
	echo /dev/${drivename}l /$lname ffs rw 1 5 | sed -e s,//,/, >> $FSTAB
fi
if [ "$mname" != "" ]; then
	echo /dev/${drivename}m /$mname ffs rw 1 5 | sed -e s,//,/, >> $FSTAB
fi
if [ "$nname" != "" ]; then
	echo /dev/${drivename}n /$nname ffs rw 1 5 | sed -e s,//,/, >> $FSTAB
fi
if [ "$oname" != "" ]; then
	echo /dev/${drivename}o /$oname ffs rw 1 5 | sed -e s,//,/, >> $FSTAB
fi
if [ "$pname" != "" ]; then
	echo /dev/${drivename}p /$pname ffs rw 1 5 | sed -e s,//,/, >> $FSTAB
fi

sync
echo	" done."

echo	"OK!  The preliminary work of setting up your disk is now complete."
echo 	""
echo	"The remaining tasks are:"
echo	""
echo	"To copy a OpenBSD kernel to the hard drive's root filesystem."
echo	"Once accomplished, you can boot off the hard drive."
echo	""
echo	"To load and install the OpenBSD distribution sets."
echo	"Currently the hard drive's root filesystem is mounted on /mnt"
echo	""
echo	"Consult the installation notes which will describe how to"
echo	"install the distribution sets and kernel.  Post-installation"
echo	"configuration is also discussed therein."
echo	""
echo	"GOOD LUCK!"
echo	""
