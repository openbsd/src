#!/bin/sh
# $OpenBSD: install.sh,v 1.4 1998/09/23 07:30:57 todd Exp $

# XXX should handle --unlink

INSTALLTO=/mnt
TARFLAGS=xfp
TARLIST="bin sbin usr.bin usr.games usr.include usr.lib usr.libexec usr.misc \
    usr.sbin usr.share var dev"

umask 0

# Usage 'install.sh [-v] [installto]'
#	-v	  : Set verbose mode (tar lists files)
#	installto : Sets the path to install to.
#
while [ -n "$1" ]; do
	case "$1" in
	-v)
		TARFLAGS=xvfp
		echo "(Verbose mode on)"
		;;
	*)
		if [ ! -d $1 ]; then
			echo "Cannot extract to $1 - no such directory."
			exit 3
		fi
		INSTALLTO=$1
		;;
	esac
	shift
done
echo "Installing to '$INSTALLTO'."

# Try for gnutar first, then gtar, or just use tar if no luck
# Also check we have gzip in the path
#
oldIFS="$IFS"
IFS=":${IFS}"
for search in gnutar gtar tar;do
	TAR=
	for dir in $PATH;do
		if [ -x $dir/$search ];then
			TAR=$dir/$search
			break 2
		fi
	done
done
echo "Using $TAR to extract files."
GZIP=
for dir in $PATH;do
	if [ -x $dir/gzip ];then
		GZIP=$dir/gzip
		break
	fi
done
if [ -z "$GZIP" ];then
	echo "Cannot find gzip in your path - aborting."
	exit 3
fi
echo "Found $GZIP."
IFS="$oldIFS"

# If etc/passwd found, check if we _really_ want to extract over etc.
#
INSTALLETC=y
if [ -f  $INSTALLTO/etc/passwd ];then
	echo ""
	echo ""
	echo "**** etc/passwd found - do you want to extract over etc? (y/n)"
	read ans
	case $ans in
	y* | Y* )
		echo "Ok - extracting etc."
		;;
	*)
		echo "Not extracting etc."
		INSTALLETC=n
		;;
	esac
	echo ""
fi
if [ $INSTALLETC = y ];then
	TARLIST="etc $TARLIST"
fi

# Extract the files
#
for f in $TARLIST ; do
	echo "---- $f ----"
	$GZIP -d -c ./$f.tar.gz | (cd $INSTALLTO; $TAR $TARFLAGS -)
	if [ $? != 0 ] ; then
		echo "**** Extract of $f failed. Aborting".
		exit 3
	fi
done

# Tidy up
#
echo "Installing bsd.scsi3 as kernel, replace by hand you need to."
cp ./bsd.scsi3 $INSTALLTO/bsd
chmod 640 $INSTALLTO/bsd; chown root.kmem $INSTALLTO/bsd
if [ ! -c $INSTALLTO/dev/console ];then
	echo "Tar did not understand device nodes - removing"
	rm -rf $INSTALLTO/dev/[a-z]*
fi
cd $INSTALLTO/dev; ./MAKEDEV all
if [ $INSTALLETC = y ];then
	mv $INSTALLTO/etc/fstab.sd $INSTALLTO/etc/fstab
fi
echo "You should probably edit $INSTALLTO/etc/fstab"
