#!/bin/sh
#
# $NetBSD: inst.sh,v 1.1 1996/05/16 19:59:05 mark Exp $
#
# Copyright (c) 1995-1996 Mark Brinicombe
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
#	This product includes software developed by Mark Brinicombe.
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
#	from inst,v 1.9 1996/04/18 20:56:14 mark Exp $
#

#
# Installation utilites (functions), to get NetBSD installed on
# the hard disk.  These are meant to be invoked from the shell prompt,
# by people installing NetBSD.
#

VERSION=`echo '$Revision: 1.1 $' | cut '-d ' -f2`

Load_tape()
{
	Tmp_dir
	echo -n	"Which tape drive will you be using? [rst0] "
	read which
	if [ "X$which" = "X" ]; then
		which=rst0
	fi
	echo -n "Insert the tape into the tape drive and hit return to "
	echo -n "continue..."
	read foo
	echo	"Extracting files from the tape..."
	$TAR xvpf --unlink /dev/$which
	echo	"Done."
}


Get_Dest_Dir()
{
	echo -n "Enter new path to dest directory [$dest_dir] "
	read newdir
	if [ ! "X$newdir" = "X" ]; then
		dest_dir=$newdir
	fi
}


Get_Distrib_Dir()
{
	echo -n "Enter new path to distrib directory [$distribdir] "
	read distribdir
	if [ "X$distribdir" = "X" ]; then
		distribdir=$destdir/usr/distrib
	fi
	if [ ! -d $distribdir ]; then
		echo -n "$distribdir does not exist, create it [n] "
		read yorn
		if [ "$yorn" = "y" ]; then
			mkdir $distribdir
		fi
	fi
}


Set_Distrib_Dir()
{
	while [ ! -d $distribdir ]; do
		echo "Directory $distribdir does not exist"
		Get_Distrib_Dir
	done

	cd $distribdir
}


Load_Msdos_Fd()
{
	echo "Loading sets from msdos floppies"
	Set_Distrib_Dir

	which=
	while [ "$which" != "0" -a "$which" != "1" ]; do
		echo -n	"Read from which floppy drive ('0' or '1')? [0] "
		read which
		if [ "X$which" = "X" ]; then
			which=0
		fi
	done

	foo=
	while [ "X$foo" = "X" ]; do
	       	echo -n "Insert floppy (type s to stop, enter to load): "
		read foo
		if [ "X$foo" = "X" ]; then
			mount -t msdos /dev/fd${which}a /mnt2
			cp -rp /mnt2/* .
			umount /mnt2
		fi
	done	
}


Load_Tar_Fd()
{
	echo "Loading sets from tar floppies"
	Set_Distrib_Dir

	which=
	while [ "$which" != "0" -a "$which" != "1" ]; do
		echo -n	"Read from which floppy drive ('0' or '1')? [0] "
		read which
		if [ "X$which" = "X" ]; then
			which=0
		fi
	done

	foo=
	while [ "X$foo" = "X" ]; do
	       	echo -n "Insert floppy (type s to stop, enter to load): "
		read foo
		if [ "X$foo" = "X" ]; then
			tar xf /dev/fd${which}a
		fi
	done	
}


Load_Tar_Fd1()
{
	echo "Loading sets from multi-volume tar floppies"
	Set_Distrib_Dir

	which=
	while [ "$which" != "0" -a "$which" != "1" ]; do
		echo -n	"Read from which floppy drive ('0' or '1')? [0] "
		read which
		if [ "X$which" = "X" ]; then
			which=0
		fi
	done

	foo=
	while [ "X$foo" = "X" ]; do
	       	echo -n "Insert floppy (type s to stop, enter to load): "
		read foo
		if [ "X$foo" = "X" ]; then
			tar xfM /dev/fd${which}a
		fi
	done	
}


Load_Sets()
{
	res0=
	while [ "$res0" != "q" -a "$res0" != "Q" ]; do
		echo ""
		echo -n "1. Set distrib directory (Currently $distribdir"
		if [ ! -d $distribdir ]; then
			echo " - non-existant)"
		else
			echo ")"
		fi
		echo "2. Load sets from msdos floppies"
		echo "3. Load sets from tar floppies"
		echo "4. Load sets from multi-volume tar floppies"
		echo "Q. Return to previous menu"
		echo ""
		echo -n "Choice : "
		read res0
		case "$res0" in
		1)
			Get_Distrib_Dir
			;;
		2)
			Load_Msdos_Fd
			;;
		3)
			Load_Tar_Fd
			;;
		4)
			Load_Tar_Fd1
			;;
		q|Q)
#			echo "Returning to previous menu"
			;;
		esac
	done
}


Mount_SCSI_CDROM()
{
	which=
	while [ "$which" != "0" -a "$which" != "1" ]; do
		echo -n	"Mount which CDROM drive ('0' or '1')? [0] "
		read which
		if [ "X$which" = "X" ]; then
			which=0
		fi
	done

	mount -r -t cd9660 /dev/cd${which}a /cdrom
	if [ ! $? = 0 ]; then
		echo "Mount failed"
	else
		distribdir="/cdrom/usr/distrib"
	fi
	
}


Mount_SCSI_Disc()
{
	echo -n	"Mount which SCSI device as CDROM ? [sd0a] "
	read which

	mount -r -t cd9660 /dev/${which} /cdrom
	if [ ! $? = 0 ]; then
		echo "Mount failed"
	else
		distribdir="/cdrom/usr/distrib"
	fi
	
}


Mount_ATAPI_CDROM()
{
	mount -r -t cd9660 /dev/wcd0a /cdrom
	if [ ! $? = 0 ]; then
		mount -r -t cd9660 /dev/wcd0a /cdrom
		if [ ! $? = 0 ]; then
			echo "Mount failed"
		else
			distribdir="/cdrom/usr/distrib"
		fi
	else
		distribdir="/cdrom/usr/distrib"
	fi
	
}


CDROM_Sets() {
	res0=
	while [ "$res0" != "q" -a "$res0" != "Q" ]; do
		echo ""
		echo -n "1. Set distrib directory (Currently $distribdir"
		if [ ! -d $distribdir ]; then
			echo " - non-existant)"
		else
			echo ")"
		fi
		echo -n "2. Mount SCSI CDROM "
		if [ ! -b /dev/cd0a ]; then
			echo "- Not available"
		else
			echo ""
		fi
		echo -n "3. Mount ATAPI CDROM "
		if [ ! -b /dev/wcd0a ]; then
			echo "- Not available"
		else
			echo ""
		fi
		if [ -f /var/inst/developer ]; then
			echo "D. Mount SCSI disc as CDROM"
		fi
		echo "Q. Return to previous menu"
		echo ""
		echo -n "Choice : "
		read res0
		case "$res0" in
		1)
			Get_Distrib_Dir
			;;
		2)
			Mount_SCSI_CDROM
			;;
		3)
			Mount_ATAPI_CDROM
			;;
		d|D)
			Mount_SCSI_Disc
			;;
		q|Q)
			;;
		esac
	done
}


Show_Sets()
{
	Set_Distrib_Dir
	sets=`ls *.[sS][eE][tT] 2>/dev/null | sed -e 's/.[sS][eE][tT]//'`

	echo "Loaded sets in $distribdir :"
	for set in $sets; do
		cat "$set".set 2>/dev/null | grep 'desc:' | cut -f2-
		cat "$set".SET 2>/dev/null | grep 'desc:' | cut -f2-
	done
}


Show_Installed_Sets()
{
	INSTALLDIR=$dest_dir/var/inst/installed
	cd $INSTALLDIR
	if [ ! $? = 0 ]; then
		return
	fi
	sets=`ls *.[sS][eE][tT] 2>/dev/null | sed -e 's/.[sS][eE][tT]//'`

	echo "Installed sets:"
	for set in $sets; do
		cat "$set".set 2>/dev/null | grep 'desc:' | cut -f2-
		cat "$set".SET 2>/dev/null | grep 'desc:' | cut -f2-
	done
}


Validate_Sets()
{
	Set_Distrib_Dir
	sets=`ls *.[sS][eE][tT] 2>/dev/null | sed -e 's/.[sS][eE][tT]//'`
	list=""

	echo "Loaded sets in $distribdir :"
	for set in $sets; do
		printf "$set\t"
		list="$list $set"
	done
	echo ""

	echo -n "Enter set names or 'all' for all sets : "
	read res2
	res2=`echo "$res2" | tr "," " " | tr "[A-Z]" "[a-z]"`
	if [ "$res2" = "all" ]; then
		res2="$list"
	fi
	sets="$res2"

	echo "Validating in $distribdir :"
	fail=""
	for set in $sets; do
		setname="set"
		if [ ! -f "$set"."$setname" ]; then
			setname="SET"
			set=`echo $set | tr [a-z] [A-Z]`
		fi
		p1=`cat $set.$setname 2>/dev/null | grep 'parts:' | cut -f2`
		if [ "$p1" = "" ]; then
			continue
		fi
		p2=`ls "$set".[0-9a-z][0-9a-z] 2>/dev/null | wc | awk '{print $1}'`
		printf "$set\t:"
#		echo "$p1 , $p2"
		if [ ! "$p1" = "$p2" ]; then
			echo -n " Failed parts check (need $p1, got $p2)"
		else
			echo -n " Passed parts check"
		fi
		loop=0
		while [ $loop -lt $p1 ]; do
			if [ $loop -lt 10 ]; then
				file="0$loop"
			else
				file=$loop
			fi
			echo -n " [$file]"
			cksum=`cat $set.$setname | grep "cksum.$file:" | cut -f2`
			cksum1=`cat "$set".$file 2>&1 | cksum | cut "-d " -f1`
			#echo "#$cksum, $cksum1#"
			if [ ! "$cksum" = "$cksum1" ]; then
				echo -n " part $file failed checksum"
				fail="yes"
			fi
			loop=$(($loop+1))
		done
		if [ "$fail" = "" ]; then
			echo " Passed checksum"
		else
			echo ""
		fi
	done
}


Verify_Sets()
{
	Set_Distrib_Dir
	sets=`ls *.[sS][eE][tT] 2>/dev/null | sed -e 's/.[sS][eE][tT]//'`
	list=""

	echo "Loaded sets in $distribdir :"
	for set in $sets; do
		printf "$set\t"
		list="$list $set"
	done
	echo ""

	echo -n "Enter set names or 'all' for all sets : "
	read res2
	res2=`echo "$res2" | tr "," " " | tr "[A-Z]" "[a-z]"`
	if [ "$res2" = "all" ]; then
		res2="$list"
	fi
	sets="$res2"

	echo "Verifing sets in $distribdir :"
	for set in $sets; do
		setname="set"
		if [ ! -f "$set"."$setname" ]; then
			setname="SET"
			set=`echo $set | tr [a-z] [A-Z]`
		fi
		printf "$set\t:"
		cat "$set".[0-9a-z][0-9a-z] 2>/dev/null | $GUNZIP -t 2>/dev/null 1>/dev/null
		if [ ! $? = 0 ]; then
			echo " Failed archive integrity"
		else
			echo " Passed"
		fi
	done
}


Verify_Checksums()
{
	echo -n "Enter name of checksums file : "
	read checkfile
	if [ "X$checkfile" = "X" ]; then
		return
	fi

	Set_Distrib_Dir
	sets=`ls *.[sS][eE][tT] 2>/dev/null | sed -e 's/.[sS][eE][tT]//'`
	list=""

	echo "Available sets in $distribdir :"
	for set in $sets; do
		printf "$set\t"
		list="$list $set"
	done
	echo ""

	echo -n "Enter set names or 'all' for all sets : "
	read res2
	res2=`echo "$res2" | tr "," " " | tr "[A-Z]" "[a-z]"`
	if [ "$res2" = "all" ]; then
		res2="$list"
	fi
	sets="$res2"

	TMPFILE1="/tmp/inst.cksum1"
	TMPFILE2="/tmp/inst.cksum2"

	echo "Verifing checksums for sets in $distribdir :"
	for set in $sets; do
		setname="set"
		if [ ! -f "$set"."$setname" ]; then
			setname="SET"
			set=`echo $set | tr [a-z] [A-Z]`
		fi
		printf "$set\t:"
		grep cksum $set.$setname >$TMPFILE1
		egrep -i "`echo ^$set.$setname | sed -e 's/\+/\\\\+/g'`" $checkfile | cut '-d:' -f2- >$TMPFILE2
		cmp -s $TMPFILE1 $TMPFILE2
		if [ $? = 0 ]; then
			echo "checksums ok"
		else
			echo "checksum error"
		fi
	done
	rm -f $TMPFILE1
	rm -f $TMPFILE2
}


Check_Sets() {
	res0=
	while [ "$res0" != "q" -a "$res0" != "Q" ]; do
		echo ""
		echo "1. Validate distribution sets (confirm checksums)"
		echo "2. Verify distribution sets (integrity check)"
		echo "3. Verify checksums (confirm set checksums)"
		echo "Q. Return to previous menu"
		echo ""
		echo -n "Choice : "
		read res0
		case "$res0" in
		1)
			Validate_Sets
			;;
		2)
			Verify_Sets
			;;
		3)
			Verify_Checksums
			;;
		q|Q)
			;;
		esac
	done
}




List_Sets()
{
	Set_Distrib_Dir
	sets=`ls *.[sS][eE][tT] 2>/dev/null | sed -e 's/.[sS][eE][tT]//'`
	list=""

	echo "Loaded sets in $distribdir :"
	for set in $sets; do
		printf "$set\t"
		list="$list $set"
	done
	echo ""

	echo -n "Enter set names or 'all' for all sets : "
	read res2
	res2=`echo "$res2" | tr "," " " | tr "[A-Z]" "[a-z]"`
	if [ "$res2" = "all" ]; then
		res2="$list"
	fi
	sets="$res2"

	echo "Listing contents of sets in $distribdir :"
	for set in $sets; do
		setname="set"
		if [ ! -f "$set"."$setname" ]; then
			setname="SET"
			set=`echo $set | tr [a-z] [A-Z]`
		fi
		echo "$set:"
		cat "$set".[0-9a-z][0-9a-z] | $GUNZIP | $TAR -tpvf -
	done
}


Install_Sets()
{
# Make sure all the directories exist for recording the installation

	INSTALLDIR=$dest_dir/var/inst
	if [ ! -d $dest_dir/var ]; then
		echo "Creating $dest_dir/var"
		mkdir $dest_dir/var
	fi
	if [ ! -d $INSTALLDIR ]; then
		echo "Creating $INSTALLDIR"
		mkdir $INSTALLDIR
	fi
	if [ ! -d $INSTALLDIR/installed ]; then
		echo "Creating $INSTALLDIR/installed"
		mkdir $INSTALLDIR/installed
	fi
	if [ ! -d $INSTALLDIR/sets ]; then
		echo "Creating $INSTALLDIR/sets"
		mkdir $INSTALLDIR/sets
	fi

	if [ ! -d $dest_dir/tmp ]; then
		echo "Creating $dest_dir/tmp"
		mkdir $dest_dir/tmp
		chmod 1777 $dest_dir/tmp
	fi

# Set the distribution directory and list all the available sets

	Set_Distrib_Dir
	sets=`ls *.[sS][eE][tT] 2>/dev/null | sed -e 's/.[sS][eE][tT]//'`
	list=""

	echo "Loaded sets in $distribdir :"
	for set in $sets; do
		printf "$set\t"
		list="$list $set"
	done
	echo ""

# Prompt for the sets to install

	echo -n "Enter set names or 'all' for all sets : "
	read res2
	res2=`echo "$res2" | tr "," " " | tr "[A-Z]" "[a-z]"`

# Check for any of the special sets

	if [ "$res2" = "all" ]; then
		res2="$list"
	fi

	if [ "$res2" = "req" ]; then
		res2="base etc misc text kern"
	fi

	if [ "$res2" = "std" ]; then
		res2="base etc misc text local man kern"
	fi

# Hack to make sure the base set is the first installed

	echo "$res2" | grep base 2>/dev/null >/dev/null
	if [ $? = 0 ]; then
		res2="base "`echo "$res2" | sed 's/base//'`
	fi

	echo "$res2" | grep BASE 2>/dev/null >/dev/null
	if [ $? = 0 ]; then
		res2="BASE "`echo "$res2" | sed 's/BASE//'`
	fi

	echo -n "Would you like to list the files as they're extracted? [n] "
	read verbose
	case $verbose in
	y*|Y*)
		tarverbose=v
		;;
	*)
		tarverbose=
		;;
	esac

	for set in $res2; do
		setname="set"
		if [ ! -f "$set"."$setname" ]; then
			setname="SET"
			set=`echo $set | tr [a-z] [A-Z]`
		fi

# Test for conflicts

		ok=""
		for file in `ls $INSTALLDIR/installed/*.[sS][eE][tT] 2>/dev/null`; do
			conflict="`grep conflict: $file | cut -d: -f2- 2>/dev/null`"
			echo $conflict | egrep -i "(^|[[:space:]])$set([[:space:]]|$)" 2>/dev/null > /dev/null
			if [ $? = 0 ]; then
				echo "Set $set conflicts with installed set `basename $file`"
				ok="no"
				break
			fi
		done

		if [ ! "$ok" = "" ]; then
			continue
		fi

		conflicts="`grep conflict: $set.$setname | cut -d: -f2-`"
		installed="`(cd $INSTALLDIR/installed ; ls *.[sS][eE][tT]) 2>/dev/null`"

		for conflict in $conflicts ; do
			echo $installed | egrep -i "(^|[[:space:]])$conflict.set([[:space:]]|$)" 2>/dev/null > /dev/null
			if [ $? = 0 ]; then
				echo "Set $set conflicts with installed set $conflict"
				ok="no"
				break;
			fi
		done

		if [ ! "$ok" = "" ]; then
			continue
		fi

		dependancies="`grep depend: $set.$setname | cut -d: -f2-`"
		installed="`(cd $INSTALLDIR/installed ; ls *.[sS][eE][tT]) 2>/dev/null`"

		for depend in $dependancies ; do
			echo $installed | egrep -i "(^|[[:space:]])$depend\.set([[:space:]]|$)" 2> /dev/null >/dev/null
			if [ ! $? = 0 ]; then
				echo "Set $depend must be installed before $set"
				ok="no"
				break;
			fi
		done

		if [ ! "$ok" = "" ]; then
			continue
		fi

		if [ -f "$set".00 ]; then
			if [ -x "$INSTALLDIR/scripts/$set" ]; then
				echo "Running deinstallation script for $set"
				(cd $dest_dir ; INSTALLROOT=$dest_dir; export INSTALLROOT ; $INSTALLDIR/scripts/$set deinstall)
			fi

			filelist="$dest_dir/tmp/$set.files"
			echo "Installing $set"
			case $verbose in
			y*|Y*)
				cat "$set".[0-9a-z][0-9a-z] | $GUNZIP | (cd $dest_dir ; $TAR  --unlink -xpvf - | tee $filelist)
				;;
			*)
				cat "$set".[0-9a-z][0-9a-z] | $GUNZIP | (cd $dest_dir ; $TAR  --unlink -xpvf - >$filelist)
				;;
			esac
			echo "Generating installation information"
			cat $filelist | awk '{ print $0 }' > $INSTALLDIR/installed/$set.files
#			cat $filelist | awk '{ print $3 $8 }' > $INSTALLDIR/installed/$set.files
			rm $filelist
			cp $set.$setname $INSTALLDIR/installed/

# Run any install script

			if [ -x "$INSTALLDIR/scripts/$set" ]; then
				echo "Running installation script for $set"
				(cd $dest_dir ; INSTALLROOT=$dest_dir; export INSTALLROOT ; $INSTALLDIR/scripts/$set install)
			fi
		else
			echo "Set $set not available for installation"
		fi
		sync
	done

# Test for other set requirements

	echo "Checking installed set requirements"

	installed="`(cd $INSTALLDIR/installed ; ls *.[sS][eE][tT]) 2>/dev/null`"

	for set in $installed; do
		required="`grep req: $INSTALLDIR/installed/$set | cut -d: -f2- 2>/dev/null`"
		for require in $required; do
			echo $installed | egrep -i "(^|[[:space:]])$require.set([[:space:]]|$)" 2>/dev/null > /dev/null
			if [ ! $? = 0 ]; then
				echo "Set $set requires set $require"
#				break
			fi
		done
	done
}


Deinstall_Sets()
{
# Make sure all the directories exist for recording the installation

	INSTALLDIR=$dest_dir/var/inst

# Set the distribution directory and list all the available sets

	cd $INSTALLDIR/installed
	sets=`ls *.[sS][eE][tT] 2>/dev/null | sed -e 's/.[sS][eE][tT]//'`
	list=""

	echo "Installed sets :"
	for set in $sets; do
		printf "$set\t"
		list="$list $set"
	done
	echo ""

# Prompt for the sets to deinstall

	echo -n "Enter set names : "
	read res2
	res2=`echo "$res2" | tr "," " " | tr "[A-Z]" "[a-z]"`

	echo -n "Would you like to list the files as they're removed? [n] "
	read verbose

	for set in $res2; do
		setname="set"
		if [ ! -f "$set"."$setname" ]; then
			setname="SET"
			set=`echo $set | tr [a-z] [A-Z]`
		fi

		if [ -f "$set".set ]; then
# Test for dependancies

			ok=""
			for file in `ls $INSTALLDIR/installed/*.[sS][eE][tT] 2>/dev/null`; do
				depend="`grep depend: $file 2>/dev/null`"
				echo $depend | egrep -i "(^|[[:space:]])$set([[:space:]]|$)" 2>/dev/null > /dev/null
				if [ $? = 0 ]; then
					echo "Installed set $file depends on set $set"
					ok="no"
					break
				fi
			done

			if [ ! "$ok" = "" ]; then
				continue
			fi

			echo "Deinstalling $set"
			if [ -x "$INSTALLDIR/scripts/$set" ]; then
				echo "Running deinstallation script for $set"
				(cd $dest_dir ; INSTALLROOT=$dest_dir; export INSTALLROOT ; $INSTALLDIR/scripts/$set deinstall)
			fi
			case $verbose in
			y*|Y*)
				for file in `cat $set.files`; do
					echo $file
					(cd $dest_dir ; rm $file) 2>/dev/null >/dev/null
				done
				;;
			*)
				cat "$set.files" | (cd $dest_dir ; xargs rm 2>/dev/null >/dev/null )
				;;
			esac
			rm "$set.files"
			rm "$set.set"
		else
			echo "Set $set not available for deinstallation"
		fi
		sync
	done

# Test for other set requirements

	echo "Checking installed set requirements"

	installed="`(cd $INSTALLDIR/installed ; ls *.[sS][eE][tT]) 2>/dev/null`"

	for set in $installed; do
		required="`grep req: $INSTALLDIR/installed/$set | cut -d: -f2- 2>/dev/null`"
		for require in $required; do
			echo $installed | egrep -i "(^|[[:space:]])$require.set([[:space:]]|$)" 2>/dev/null > /dev/null
			if [ ! $? = 0 ]; then
				echo "Set $set requires set $require"
#				break
			fi
		done
	done
}


Select_Sets()
{
}


Main_Menu()
{
	res1=
	while [ "$res1" != "q" -a "$res1" != "Q" ]; do
		echo ""
		echo "RiscBSD (NetBSD/arm32) Installer V$VERSION"
		echo ""
		echo -n "1. Set distrib directory (Currently $distribdir"
		if [ ! -d $distribdir ]; then
			echo " - non-existant)"
		else
			echo ")"
		fi
		echo "2. Load distribution sets onto harddisc"
		echo "3. Load distribution sets from CDROM"
		echo "4. Show distribution sets"
		echo "5. Show installed sets"
		echo "6. Check sets (verify/validate)"
		echo "7. List contents of distribution sets in $distribdir"
		echo "8. Install distribution sets from $distribdir"
		echo "D. Deinstall installed sets"
		echo "R. Set root directory for install (Currently $dest_dir)"
		echo "Q. Quit"
		echo ""
		echo -n "Choice : "
		read res1
		case "$res1" in
		1)
			Get_Distrib_Dir
			;;
		2)
			Load_Sets
			;;
		3)
			CDROM_Sets
			;;
		4)
			Show_Sets
			;;
		5)
			Show_Installed_Sets
			;;
		6)
			Check_Sets
			;;
		7)
			List_Sets
			;;
		8)
			Install_Sets
			;;
		d|D)
			Deinstall_Sets
			;;
		r|R)
			Get_Dest_Dir
			;;
		q|Q)
#			echo "Quitting"
			;;
		esac
	done
}


# we know that /etc/fstab is only generated on the hard drive
destdir=
dest_dir=/
if [ ! -f /etc/fstab ]; then
	dest_dir=/mnt/
fi
if [ -f /etc/cdrom ]; then
	dest_dir=/mnt/
fi

# counter for possible shared library confusion
TAR=/usr/bin/tar
GUNZIP=/usr/bin/gunzip

distribdir=$destdir/usr/distrib

#IAM=`whoami`
#
#if [ ! "$IAM" = "root" ]; then
#	echo "inst should be run as root"
#	exit
#fi

Main_Menu

exit
