#!/bin/sh
#	$OpenBSD: binstall.sh,v 1.4 1996/08/11 09:13:21 downsj Exp $
#	$NetBSD: binstall.sh,v 1.3 1996/04/07 20:00:12 thorpej Exp $
#

vecho () {
# echo if VERBOSE on
	if [ "$VERBOSE" = "1" ]; then
		echo "$@" 1>&2
	fi
	return 0
}

Usage () {
	echo "Usage: $0 [-hvt] [-m<path>] net|ffs directory"
	exit 1
}

Help () {
	echo "This script copies the boot programs to one of several"
	echo "commonly used places. It takes care of stripping the"
	echo "a.out(5) header off the installed boot program on sun4 machines."
	echo "When installing an \"ffs\" boot program, this script also runs"
	echo "installboot(8) which installs the default proto bootblocks into"
	echo "the appropriate filesystem partition."
	echo "Options:"
	echo "	-h		- display this message"
	echo "	-m<path>	- Look for boot programs in <path> (default: /usr/mdec)"
	echo "	-v		- verbose mode"
	echo "	-t		- test mode (implies -v)"
	exit 0
}


PATH=/bin:/usr/bin:/sbin:/usr/sbin
MDEC=${MDEC:-/usr/mdec}

set -- `getopt "hm:tv" "$@"`
if [ $? -gt 0 ]; then
	Usage
fi

for a in $*
do
	case $1 in
	-h) Help; shift ;;
	-m) MDEC=$2; shift 2 ;;
	-t) TEST=1; VERBOSE=1; shift ;;
	-v) VERBOSE=1; shift ;;
	--) shift; break ;;
	esac
done

DOIT=${TEST:+echo "=>"}

if [ $# != 2 ]; then
	Usage
fi

WHAT=$1
DEST=$2

if [ "`sysctl -n hw.model | cut -b1-5`" = "SUN-4" ]; then
	KARCH=sun4
else
	KARCH=sun4c
fi
vecho "Kernel architecture: $KARCH"

if [ ! -d $DEST ]; then
	echo "$DEST: not a directory"
	Usage
fi


if [ $KARCH = sun4 ]; then SKIP=1; else SKIP=0; fi


case $WHAT in
"ffs")
	DEV=`mount | while read line; do
		set -- $line
		vecho "Inspecting \"$line\""
		if [ "$2" = "on" -a "$3" = "$DEST" ]; then
			if [ ! -b $1 ]; then
				continue
			fi
			RAW=\`echo -n "$1" | sed -e 's;/dev/;/dev/r;'\`
			if [ ! -c \$RAW ]; then
				continue
			fi
			echo -n $RAW
			break;
		fi
	done`
	if [ "$DEV" = "" ]; then
		echo "Cannot find \"$DEST\" in mount table"
		exit 1
	fi
	TARGET=$DEST/boot
	vecho Boot device: $DEV
	vecho Target: $TARGET
	$DOIT dd if=${MDEC}/boot of=$TARGET skip=$SKIP bs=32
	sync; sync; sync
	vecho installboot ${VERBOSE:+-v} $TARGET ${MDEC}/bootxx $DEV
	$DOIT installboot ${VERBOSE:+-v} $TARGET ${MDEC}/bootxx $DEV
	;;

"net")
	TARGET=$DEST/boot.sparc.openbsd.$KARCH
	vecho Target: $TARGET
	$DOIT dd if=${MDEC}/boot of=$TARGET skip=$SKIP bs=32
	;;

*)
	echo "$WHAT: not recognised"
	exit 1
	;;
esac

exit $?
