#!/bin/sh

# $OpenBSD: fw_update.sh,v 1.21 2014/02/23 22:22:16 halex Exp $
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

PKG_ADD="pkg_add -I -D repair -DFW_UPDATE"
PKG_DELETE="pkg_delete -I -DFW_UPDATE"

usage() {
	echo "usage: ${0##*/} [-adnv] [-p path] [driver ...]" >&2
	exit 1
}

verbose() {
	[ "$verbose" ] && echo "${0##*/}: $@"
}

setpath() {
	[ "$path" ] && export PKG_PATH=$path && return

	set -- $(sysctl -n kern.version |
	    sed 's/^OpenBSD \([0-9]\.[0-9]\)\([^ ]*\).*/\1 \2/;q')

	local version=$1 tag=$2

	[[ $tag == -!(stable) ]] && version=snapshots
	export PKG_PATH=http://firmware.openbsd.org/firmware/$version/
}

perform() {
	if [ "$verbose" ]; then
		"$@"
	else
		"$@" 2>/dev/null
	fi
}

all=false
verbose=
nop=
delete=false
path=
while getopts 'adnp:v' s "$@" 2>/dev/null; do
	case "$s" in
	a)	all=true;;
	d)	delete=true;;
	n)	nop=-n;;
	p)	path=$OPTARG;;
	v)	verbose=${verbose:--}v;;
	*)	usage;;
	esac
done

shift $((OPTIND - 1))

if $all; then
	[ $# != 0 ] && usage
	set -- $DRIVERS
fi

if $delete && [ $# == 0 ]; then
	echo "Driver specification required for delete operation" >&2
	exit 1
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

if ! $delete && [ -z "$install$update" ]; then
	verbose "No devices found which need firmware files to be downloaded."
	exit 0
elif $delete && [ -z "$update" ]; then
	verbose "No firmware to delete."
	exit 0
fi

$delete || verbose "Path to firmware: $PKG_PATH"

[ "$nop" ] || [ 0 = $(id -u) ] ||
	{ echo "${0##*/} must be run as root" >&2; exit 1; }

# Install missing firmware packages
if ! $delete && [ "$install" ]; then
	verbose "Installing firmware:$update $install."
	perform $PKG_ADD $nop $verbose $update $install
fi

# Update or delete installed firmware packages
if [ "$update" ]; then
	if $delete; then
		verbose "Deleting firmware:$update."
		perform $PKG_DELETE $nop $verbose $update
	else
		verbose "Updating firmware:$update."
		perform $PKG_ADD $extra $nop $verbose -u $update
	fi
fi
