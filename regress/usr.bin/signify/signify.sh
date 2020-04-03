#!/bin/sh
#
# $OpenBSD: signify.sh,v 1.10 2020/04/03 12:01:56 bluhm Exp $

srcdir=$1

pubkey="$srcdir/regresskey.pub"
seckey="$srcdir/regresskey.sec"
orders="$srcdir/orders.txt"
forgery="$srcdir/forgery.txt"

set -e

cat $seckey | signify -S -s - -x test.sig -m $orders 
diff -u "$orders.sig" test.sig

signify -V -q -p $pubkey -m $orders

signify -V -q -p $pubkey -m $forgery 2> /dev/null && exit 1

signify -S -s $seckey -x confirmorders.sig -e -m $orders 
signify -V -q -p $pubkey -e -m confirmorders
diff -u $orders confirmorders

sha256 $pubkey $seckey > HASH
sha512 $orders $forgery >> HASH
signify -S -e -s $seckey -m HASH
rm HASH
signify -C -q -p $pubkey -x HASH.sig

tar zcPf archive.tgz $srcdir/*.txt
signify -zS -s $seckey -m archive.tgz -x signed.tgz
# check it's still valid gzip
gunzip -t signed.tgz
# verify it
signify -zV -p $pubkey <signed.tgz|signify -zV -p $pubkey|gunzip -t
true
