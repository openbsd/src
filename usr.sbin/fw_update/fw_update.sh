#!/bin/sh

# $OpenBSD: fw_update.sh,v 1.16 2013/08/20 22:42:08 halex Exp $
# Copyright (c) 2011 Alexander Hall <alexander@beard.se>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# This is the list of drivers we should look for
DRIVERS="acx athn bwi ipw iwi iwn malo otus pgt radeondrm rsu uath
	upgt urtwn uvideo wpi"

PKG_ADD="pkg_add -I -D repair"

usage() {
	echo "usage: ${0##*/} [-anv] [driver ...]" >&2
	exit 1
}

verbose() {
	[ "$verbose" ] && echo "${0##*/}: $@"
}

setpath() {
	set -- $(sysctl -n kern.version |
	    sed 's/^OpenBSD \([0-9]\.[0-9]\)\([^ ]*\).*/\1 \2/;q')

	local version=$1 tag=$2

	[[ $tag == -!(stable) ]] && version=snapshots
	export PKG_PATH=http://firmware.openbsd.org/firmware/$version/
}

all=false
verbose=
nop=
while getopts 'anv' s "$@" 2>/dev/null; do
	case "$s" in
	a)	all=true;;
	v)	verbose=${verbose:--}v ;;
	n)	nop=-n ;;
	*)	usage ;;
	esac
done

shift $((OPTIND - 1))

if $all; then
	[ $# != 0 ] && usage
	set -- $DRIVERS
fi

setpath

installed=$(pkg_info -q)
dmesg=$(cat /var/run/dmesg.boot; echo; dmesg)

install=
update=
extra=

if [ $# = 0 ]; then
	for driver in $DRIVERS; do
		if print "$installed" | grep -q "^$driver-firmware-" ||
		    print -r -- "$dmesg" | egrep -q "^$driver[0-9]+ at "; then
			set -- "$@" $driver
		fi
	done
fi

for driver; do
	if print "$installed" | grep -q "^${driver}-firmware-"; then
		update="$update ${driver}-firmware"
		extra="$extra -Dupdate_${driver}-firmware"
	elif printf "%s\n" $DRIVERS | fgrep -qx "$driver"; then
		install="$install ${driver}-firmware"
	else
		print -r "${0##*/}: $driver: unknown driver" >&2
		exit 1
	fi
done

if [ -z "$install$update" ]; then
	verbose "No devices found which need firmware files to be downloaded."
	exit 0
fi

[ "$nop" ] || [ 0 = $(id -u) ] ||
	{ echo "${0##*/} must be run as root" >&2; exit 1; }

# Install missing firmware
if [ "$install" ]; then
	verbose "Installing firmware files:$install."
	$PKG_ADD $nop $verbose $install 2>/dev/null
fi

# Update installed firmware
if [ "$update" ]; then
	verbose "Updating firmware files:$update."
	$PKG_ADD $extra $nop $verbose -u $update 2>/dev/null
fi
