#!/bin/sh
#	$OpenBSD: install.md,v 1.5 1997/04/22 10:34:41 deraadt Exp $
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

VERSION=2.1
FSTAB=${FSTABDIR}/fstab

# XXX turn into a loop which understands ! for a subshell.  Also,
# XXX is it possible to detect ^Z in a prompt and re-prompt when sh
# XXX is unsuspended?
getresp() {
	read resp
	if [ "X$resp" = "X" ]; then
		resp=$1
	fi
}

echo "Welcome to the OpenBSD ${VERSION} installation program."
echo ""
echo "This program is will put OpenBSD on your hard disk.  It is not painless"
echo "but it could be worse.  You'll be asked several questions, and it would"
echo "probably be useful to have your disk's hardware manual, the installation"
echo "notes, and a calculator handy."
echo ""
echo "In particular, you will need to know some reasonably detailed information"
echo "about your disk's geometry, because this program does not know everything."
echo ""
echo "As with anything which modifies your hard drive's contents, this program"
echo "can cause SIGNIFICANT data loss, and you are advised to make sure your"
echo "hard drive is backed up before beginning the installation process."
echo ""
echo "Default answers are displyed in brackets after the questions.  You can"
echo "hit Control-C at any time to quit.  Also, quitting towards the latter
echo "part of the installation may leave your system in an inconsistent state."
echo ""
echo -n "Proceed with installation? [n] "
getresp "n"
case "$resp" in
y*|Y*)
	echo "Cool!  Let's get to it..."
	;;
*)
	echo "OK, simply reset the machine at any time."
	exit
	;;
esac

echo ""
echo "To do the installation, you'll need to provide some information about"
echo "your disk."

echo "OpenBSD can be installed on ST506, ESDI, IDE, or SCSI disks."
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
	echo "FATAL ERROR:"
	echo "No devices for disks of type '${drivetype}'."
	echo "This is probably a bug in the install disks."
	echo "Exiting install program."
	exit
fi
prefdrive=${drivetype}0

echo "The following ${drivetype}-type disks are supported by this"
echo "installation procedure:"
echo "${driveunits}"
echo "Note that they may not exist in _your_ machine; the list of"
echo "disks in your machine was printed when the system was booting."
while [ "X${drivename}" = "X" ]; do
	echo -n	"Which disk would like to install on? [${prefdrive}] "
	getresp ${prefdrive}
	otherdrives=`echo "${driveunits}" | sed -e s,${resp},,`
	if [ "X${driveunits}" = "X${otherdrives}" ]; then
		echo "\"${resp}\" is an invalid drive name.  Valid choices"
		echo "are: "${driveunits}
	else
		drivename=${resp}
	fi
done

echo "Using disk ${drivename}."
echo -n	"What kind of disk is it? (one word please) [my${drivetype}] "
getresp "my${drivetype}"
labelname=$resp

echo ""
echo "You will now need to provide some information about your disk's geometry."
echo "This should either be in the User's Manual for your disk, or you should"
echo "have written down what OpenBSD printed when booting.  Note that the"
echo "geometry that's printed at boot time is preferred.)  You may choose to"
echo "view the initial boot messages for your system again right now if you like."
echo -n "View the boot messages again? [n] "
getresp "n"
case "$resp" in
y*|Y*)
	less -rsS /kern/msgbuf
	;;
*)
	echo ""
	;;
esac

echo "You will now enter the disk geometry information:"

bytes_per_sect=`cat /kern/msgbuf |\
    sed -n -e /^${drivename}:/p -e /^${drivename}:/q |\
    sed 's/\([^ ]*[ ]*\)\{8\}\([^ ]*\).*$/\2/'`
echo -n	"Number of bytes per disk sector? [$bytes_per_sect] "
getresp $bytes_per_sect
bytes_per_sect="$resp"

cyls_per_disk=`cat /kern/msgbuf |\
    sed -n -e /^${drivename}:/p -e /^${drivename}:/q |\
    sed 's/\([^ ]*[ ]*\)\{2\}\([^ ]*\).*$/\2/'`
echo -n "Number of disk cylinders? [$cyls_per_disk]"
getresp $cyls_per_disk
cyls_per_disk="$resp"

tracks_per_cyl=`cat /kern/msgbuf |\
    sed -n -e /^${drivename}:/p -e /^${drivename}:/q |\
    sed 's/\([^ ]*[ ]*\)\{4\}\([^ ]*\).*$/\2/'`
echo -n	"Number of disk tracks (heads) per disk cylinder? [$tracks_per_cyl]"
getresp $tracks_per_cyl
tracks_per_cyl="$resp"

sects_per_track=`cat /kern/msgbuf |\
    sed -n -e /^${drivename}:/p -e /^${drivename}:/q |\
    sed 's/\([^ ]*[ ]*\)\{6\}\([^ ]*\).*$/\2/'`
echo -n	"Number of disk sectors per disk track? [$sects_per_track]"
getresp $sects_per_track
sects_per_track="$resp"

cylindersize=`expr $sects_per_track \* $tracks_per_cyl`
cylbytes=`expr $cylindersize \* $bytes_per_sect`
disksize=`expr $cylindersize \* $cyls_per_disk`

echo ""
echo "Your disk has a total of $disksize $bytes_per_sect byte sectors,"
echo "arranged as $cyls_per_disk cylinders which contain $cylindersize "
echo "sectors ($cylbytes bytes) each."
echo "You can specify partition sizes in cylinders ('c') or sectors ('s')."
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
		echo "Enter cylinders ('c') or sectors ('s')."
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
	echo -n "Offset of OpenBSD portion of disk (in $sizeunit)? [0]"
	getresp '0'
	part_offset=$resp
fi
badspacesec=0
if [ "$sect_fwd" = "sf:" ]; then
	badspacecyl=`expr $sects_per_track + 126`
	badspacecyl=`expr $badspacecyl + $cylindersize - 1`
	badspacecyl=`expr $badspacecyl / $cylindersize`
	badspacesec=`expr $badspacecyl \* $cylindersize`
	echo ""
	echo -n "Using $badspacesec sectors ($badspacecyl cylinders) for the "
	echo "bad144 bad block table"
fi

sects_left=`expr $partition_sects - $badspacesec`
units_left=`expr $sects_left / $sizemult`
echo ""
echo "There are $units_left $sizeunit left to allocate."
echo ""
root=0
while [ $root -eq 0 ]; do
	echo -n "Root partition size (in $sizeunit)? "
	getresp
	case $resp in
	[1-9]*)
		total=$resp
		if [ $total -gt $units_left ]; then
			echo -n	"Root size is greater than remaining "
			echo "free space on disk."
		else
			root=$resp
		fi
		;;
	esac
done
root_offset=$part_offset
part_used=`expr $root + $badspacesec / $sizemult`
units_left=`expr $partition - $part_used`
echo ""

swap=0
while [ $swap -eq 0 ]; do 
	echo "$units_left $sizeunit remaining in OpenBSD portion of disk."
	echo -n	"Swap partition size (in $sizeunit)? "
	getresp
	case $resp in
	[1-9]*)
		if [ $swap -gt $units_left ]; then
			echo -n	"Swap size is greater than remaining "
			echo "free space on disk."
		else
			swap=$resp
		fi
		;;
	esac
done
swap_offset=`expr $root_offset + $root`
part_used=`expr $part_used + $swap`
echo ""

fragsize=1024
blocksize=8192
cat /etc/disktab.preinstall > $DT
echo "" >> $DT
echo "$labelname|OpenBSD installation generated:\\" >> $DT
echo "	:dt=${type}:ty=winchester:\\" >> $DT
echo -n	"	:nc#${cyls_per_disk}:ns#${sects_per_track}" >> $DT
echo ":nt#${tracks_per_cyl}:\\" >> $DT
echo "	:se#${bytes_per_sect}:${sect_fwd}\\" >> $DT
_size=`expr $root \* $sizemult`
_offset=`expr $root_offset \* $sizemult`
echo -n	"	:pa#${_size}:oa#${_offset}" >> $DT
echo ":ta=4.2BSD:ba#${blocksize}:fa#${fragsize}:\\" >> $DT
_size=`expr $swap \* $sizemult`
_offset=`expr $swap_offset \* $sizemult`
echo "	:pb#${_size}:ob#${_offset}:tb=swap:\\" >> $DT
echo "	:pc#${disksize}:oc#0:\\" >> $DT

echo "You now must enter information about any other partitions to be created in"
echo "the OpenBSD portion of the disk.  This process will be complete when you've"
echo "filled up all remaining space in the OpenBSD portion of the disk."

while [ $part_used -lt $partition ]; do
	part_size=0
	units_left=`expr $partition - $part_used`
	while [ $part_size -eq 0 ]; do
		echo ""
		echo -n	"$units_left $sizeunit remaining in OpenBSD portion of "
		echo "the disk"
		echo -n "Next partition size (in $sizeunit) [$units_left] ? "
		getresp "$units_left"
		case $resp in
		[1-9]*)
			total=`expr $part_used + $resp`
			if [ $total -gt $partition ]; then
				echo "That would make the parition too large to fit!"
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

echo ""
echo "THIS IS YOUR LAST CHANCE!!!"
echo -n	"Are you SURE you want OpenBSD installed on your hard drive? (yes/no) "
answer=""
while [ "$answer" = "" ]; do
	getresp
	case $resp in
	yes|YES)
		answer=yes
		;;
	no|NO)
		echo "OK, then.  Simply reset your machine at any time."
		exit
		;;
	*)
		echo -n "I want a yes or no answer...  well? "
		;;
	esac
done

umount -f /mnt > /dev/null 2>&1
umount -f /mnt2 > /dev/null 2>&1

echo ""
echo "Labeling disk $drivename..."
# XXX add fdisk support
$DONTDOIT disklabel -w -B $drivename $labelname

if [ "$sect_fwd" = "sf:" ]; then
	echo "Initializing bad144 badblock table..."
	$DONTDOIT bad144 $drivename 0
fi

echo "Initializing root filesystem, and mounting..."
$DONTDOIT newfs /dev/r${drivename}a $name
$DONTDOIT mount -v /dev/${drivename}a /mnt
if [ "$ename" != "" ]; then
	echo "Initializing $ename filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}e $name
	$DONTDOIT mkdir -p /mnt/$ename
	$DONTDOIT mount -v /dev/${drivename}e /mnt/$ename
fi
if [ "$fname" != "" ]; then
	echo "Initializing $fname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}f $name
	$DONTDOIT mkdir -p /mnt/$fname
	$DONTDOIT mount -v /dev/${drivename}f /mnt/$fname
fi
if [ "$gname" != "" ]; then
	echo "Initializing $gname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}g $name
	$DONTDOIT mkdir -p /mnt/$gname
	$DONTDOIT mount -v /dev/${drivename}g /mnt/$gname
fi
if [ "$hname" != "" ]; then
	echo "Initializing $hname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}h $name
	$DONTDOIT mkdir -p /mnt/$hname
	$DONTDOIT mount -v /dev/${drivename}h /mnt/$hname
fi
if [ "$iname" != "" ]; then
	echo "Initializing $iname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}i $name
	$DONTDOIT mkdir -p /mnt/$iname
	$DONTDOIT mount -v /dev/${drivename}i /mnt/$iname
fi
if [ "$jname" != "" ]; then
	echo "Initializing $jname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}j $name
	$DONTDOIT mkdir -p /mnt/$jname
	$DONTDOIT mount -v /dev/${drivename}j /mnt/$jname
fi
if [ "$kname" != "" ]; then
	echo "Initializing $kname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}k $name
	$DONTDOIT mkdir -p /mnt/$kname
	$DONTDOIT mount -v /dev/${drivename}k /mnt/$kname
fi
if [ "$lname" != "" ]; then
	echo "Initializing $lname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}l $name
	$DONTDOIT mkdir -p /mnt/$lname
	$DONTDOIT mount -v /dev/${drivename}l /mnt/$lname
fi
if [ "$mname" != "" ]; then
	echo "Initializing $mname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}m $name
	$DONTDOIT mkdir -p /mnt/$mname
	$DONTDOIT mount -v /dev/${drivename}m /mnt/$mname
fi
if [ "$nname" != "" ]; then
	echo "Initializing $nname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}n $name
	$DONTDOIT mkdir -p /mnt/$nname
	$DONTDOIT mount -v /dev/${drivename}n /mnt/$nname
fi
if [ "$oname" != "" ]; then
	echo "Initializing $oname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}o $name
	$DONTDOIT mkdir -p /mnt/$oname
	$DONTDOIT mount -v /dev/${drivename}o /mnt/$oname
fi
if [ "$pname" != "" ]; then
	echo "Initializing $pname filesystem, and mounting..."
	$DONTDOIT newfs /dev/r${drivename}p $name
	$DONTDOIT mkdir -p /mnt/$pname
	$DONTDOIT mount -v /dev/${drivename}p /mnt/$pname
fi

echo "Populating filesystems with bootstrapping binaries and config files"
$DONTDOIT tar -cXf - . | (cd /mnt ; tar -xpf - )

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

echo "OK!  The preliminary work of setting up your disk is now complete."
echo "Currently the hard drive's root filesystem is mounted on /mnt"

echo 	""
echo "How would you like to install the distribution and kernels?"
echo -n	"ftp, http, msdos, ext2fs, tape, nfs, cd9660, local? [ftp] "
getresp "ftp"
method=${resp}
case "${method}" in
ftp|http|nfs)
	echo -n "What is your ethernet interface name? [ep0] "
	getresp "ep0"
	intf=${resp}
	echo -n "Does your ethernet interface need special flags like -link0? [] "
	getresp ""
	intflags=${resp}
	echo -n "What is your IP address? [199.185.137.99] "
	getresp "199.185.137.99"
	myip=${resp}
	echo -n "What is your IP netmask? [255.255.255.0] "
	getresp "255.255.255.0"
	mymask=${resp}
	$DONTDOIT ifconfig ${intf} inet ${myip} netmask ${mymask} ${intflags} up
	echo -n "What is your default IP router? [199.185.137.128] "
	getresp "199.185.137.128"
	myrouter=${resp}
	$DONTDOIT route add default ${myrouter}
	ftp -V -a ftp://cvs.openbsd.org/pub/OpenBSD/ftplist | cat
	echo -n "What is the remote machine to fetch from? [ftp3.usa.openbsd.org] "
	getresp "ftp3.usa.openbsd.org"
	tohost=${resp}
	#ping -c 1 ${resp}
	echo -n "What is the path to fetch from? [pub/OpenBSD/snapshots/i386] "
	getresp "pub/OpenBSD/snapshots/i386"
	# XXX add proxy support?
	topath="${resp}"
	;;

msdos|ext2fs|cd9660)
	echo -n "which disk? [$drivename] "
	getresp "$drivename"
	$DONTDOIT disklabel "${resp}"
	drive=${resp}
	echo -n "which partition? [c] "
	getresp c
	part=${drive}${resp}
	$DONTDOIT mount -t $method /dev/$part /mnt2
	echo "We pray this has not bailed, ok?"
	echo -n "enter path on the device? [/] "
	getresp "/"
	fetch="cat /mnt2/${resp}"
	;;
local)
	echo -n "enter path on the device? [/] "
	getresp "/"
	fetch="cat /${resp}"
	;;
esac

case "$method" in
nfs)
	echo "XXX"
	echo "XXX should do the NFS mount here"
	echo "XXX"
	fetch="echo"
	;;
ftp)
	fetch="ftp -a ftp://${tohost}/${topath}"
	;;
http)
	fetch="ftp -a http://${tohost}/${topath}"
	;;

esac

cd /mnt
for i in bsd; do
	$DONTDOIT eval ${fetch}/${i} > $i
done
for i in bin.tar.gz dev.tar.gz etc.tar.gz sbin.tar.gz usr.bin.tar.gz \
    usr.games.tar.gz usr.include.tar.gz usr.lib.tar.gz usr.libexec.tar.gz \
    usr.misc.tar.gz usr.sbin.tar.gz usr.share.tar.gz var.tar.gz; do
	$DONTDOIT eval ${fetch}/${i} | tar xvfzp -
done
cd /

Configure
echo ""
echo "Your hard drive is still mounted.  Be sure to halt or reboot the"
echo "machine instead of simply turning it off."
