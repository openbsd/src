#!/bin/ksh
#
# $OpenBSD: dbupdate.sh,v 1.11 2026/10/22 02:52:49 dgl Exp $
#
# Copyright (c) 2026 Theo de Raadt <deraadt@openbsd.org>
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

set -o errexit
umask 0022

PROG=${0##*/}

usage() {
        echo "usage: $PROG [-fn]"
        exit 1
}

D=$(mktemp -d /tmp/_updXXXXXXXXXX) || exit 1
trap "rm -rf $D" 0 1 2 3 13 15

FORCE=false
DRY=false

while getopts "fn" name; do
	case "$name" in
	f) FORCE=true ;;
	n) DRY=true ;;
	?) usage ;;
	esac
done

[[ $(id -u) -ne 0 ]] && echo $PROG: need root privileges && exit 1

CACHE=/var/dbupdate
R=$(sysctl -n kern.osrelease | sed -e 's/\.//')
PUBKEY=/etc/signify/openbsd-${R}-base.pub

mkdir -p ${CACHE}	# XXX created by mtree, but help systems without..
cd $D

if ! ftp -o out-SHA256.sig \
    https://ftp.openbsd.org/pub/OpenBSD/dbupdates/$R/SHA256.sig \
    > /dev/null 2>&1 ; then
	echo $PROG: no databases available.
	exit 0
fi

if cmp -s ${CACHE}/out-SHA256.sig out-SHA256.sig ; then
	if [ $FORCE != true ]; then
		echo $PROG: databases have not changed.
		exit 0
	fi
fi

# Get the new file & verify the signing
ftp https://ftp.openbsd.org/pub/OpenBSD/dbupdates/dbupdates.tgz > /dev/null
signify -C -q -p "${PUBKEY}" -x out-SHA256.sig

# extract
tar xfz dbupdates.tgz

# check if the contents are same as previously processed
if cmp -s ${CACHE}/in-SHA256 SHA256 ; then
	if [ $FORCE != true ]; then
		echo $PROG: same internals, nothing to do
		exit 0
	fi
fi
touch ${CACHE}/in-SHA256

n=$(cat SHA256 | grep '^SHA256 (zoneinfo.tar) ')
o=$(cat ${CACHE}/in-SHA256 | grep '^SHA256 (zoneinfo.tar) ' || true)
if [ $FORCE = true -o X"$o" != X"$n" ]; then
	if [ $DRY = true ]; then
		echo $PROG: would have installed new zoneinfo
	else
		tar xpf zoneinfo.tar -C /
		echo $PROG: installed new zoneinfo
	fi
fi

n=$(cat SHA256 | grep '^SHA256 (cert.tar) ')
o=$(cat ${CACHE}/in-SHA256 | grep '^SHA256 (cert.tar) ' || true)
if [ $FORCE = true -o X"$o" != X"$n" ]; then
	tar xpf cert.tar ./etc/ssl/cert.pem

	if [ $DRY == true ]; then
		echo $PROG: would have installed new cert.pem
	else
		# prepend cert.local.pem
		t=$(mktemp /etc/ssl/cert.XXXXXXXX)
		[ -f /etc/ssl/cert.local.pem ] &&
		    cat /etc/ssl/cert.local.pem > $t
		cat ./etc/ssl/cert.pem >> $t
		chmod a+r $t
		mv $t /etc/ssl/cert.pem
		echo $PROG: installed new cert.pem
	fi
fi

# finish up
if [ $DRY = false ]; then
	cp SHA256 ${CACHE}/in-SHA256
	cp out-SHA256.sig ${CACHE}/out-SHA256.sig
fi
