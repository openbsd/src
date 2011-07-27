#!/bin/sh

# $OpenBSD: fw_update.sh,v 1.7 2011/07/27 15:12:57 halex Exp $
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
DRIVERS="acx athn bwi ipw iwi iwn malo otus pgt rsu uath ueagle upgt urtwn
	uvideo wpi"

PKG_ADD="pkg_add -D repair"

usage() {
	echo "usage: ${0##*/} [-nv]" >&2
	exit 1
}

verbose() {
	[ "$verbose" ] && echo "${0##*/}: $@"
}

verbose=
nop=
while getopts 'nv' s "$@" 2>&-; do
	case "$s" in
	v)	verbose=${verbose:--}v ;;
	n)	nop=-n ;;
	*)	usage ;;
	esac
done

# No additional arguments allowed
[ $# = $(($OPTIND-1)) ] || usage

set -- $(sysctl -n kern.version | sed 's/^OpenBSD \([0-9]\.[0-9]\)\([^ ]*\).*/\1 \2/;q')

version=$1
tag=$2

[ -n "$tag" -a X"$tag" != X"-stable" ] && version=snapshots
export PKG_PATH=http://firmware.openbsd.org/firmware/$version/

installed=$(pkg_info -q)
dmesg=$(cat /var/run/dmesg.boot; echo; dmesg)

install=
update=

for driver in $DRIVERS; do
	if print -r -- "$installed" | grep -q "^${driver}-firmware-"; then
		update="$update ${driver}-firmware"
	elif print -r -- "$dmesg" | grep -q "^${driver}[0-9][0-9]* at "; then
		install="$install ${driver}-firmware"
	fi
done

if [ -z "$install$update" ]; then
	verbose "No devices found which need firmwares to be downloaded."
	exit 0
fi

[ "$nop" ] || [ 0 = $(id -u) ] ||
	{ echo "${0##*/} must be run as root" >&2; exit 1; }

# Install missing firmwares
if [ "$install" ]; then
	verbose "Installing firmwares:$install."
	$PKG_ADD $nop $verbose $install
fi

# Update installed firmwares
if [ "$update" ]; then
	verbose "Updating firmwares:$update."
	$PKG_ADD $nop $verbose -u $update
fi
