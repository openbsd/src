#!/bin/sh

set -o errexit

usage() {
        echo "usage: ${0##*/} [-fn]"
        exit 1
}

D=`mktemp -d /tmp/_updXXXXXXXXXX` || exit 1
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

CACHE=/var/dbupdate
R=$(sysctl -n kern.osrelease | sed -e 's/\.//')
PUBKEY=/etc/signify/openbsd-${R}-base.pub

mkdir -p ${CACHE}	# XXX create this in mtree
cd $D

ftp -o out-SHA256.sig \
    https://ftp.openbsd.org/pub/OpenBSD/dbupdates/$R/SHA256.sig > /dev/null

if cmp -s ${CACHE}/out-SHA256.sig out-SHA256.sig ; then
	if [ $FORCE != true ]; then
		echo dbupdates has not changed.
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
		echo same internals, nothing to do
		exit 0
	fi
fi
touch ${CACHE}/in-SHA256

n=$(cat SHA256 | grep '^SHA256 (zoneinfo.tar) ')
o=$(cat ${CACHE}/in-SHA256 | grep '^SHA256 (zoneinfo.tar) ' || true)
if [ $FORCE = true -o X"$o" != X"$n" ]; then
	if [ $DRY = true ]; then
		echo dbupdate: would have installed new zoneinfo
	else
		tar xpf zoneinfo.tar -C /
		echo dbupdate: installed new zoneinfo
	fi
fi

n=$(cat SHA256 | grep '^SHA256 (cert.tar) ')
o=$(cat ${CACHE}/in-SHA256 | grep '^SHA256 (cert.tar) ' || true)
if [ $FORCE = true -o X"$o" != X"$n" ]; then
	tar xpf cert.tar ./etc/ssl/cert.pem

	if [ $DRY == true ]; then
		echo dbupdate: would have installed new cert.pem
	else
		# prepend cert.local.pem
		t=`mktemp /etc/ssl/cert.XXXXXXXX`
		[ -f /etc/ssl/cert.local.pem ] &&
		    cat /etc/ssl/cert.local.pem > $t
		cat ./etc/ssl/cert.pem >> $t
		chmod a+r $t
		mv $t /etc/ssl/cert.pem
		echo dbupdate: installed new cert.pem
	fi
fi

# finish up
if [ $DRY = false ]; then
	cp SHA256 ${CACHE}/in-SHA256
	cp out-SHA256.sig ${CACHE}/out-SHA256.sig
fi
