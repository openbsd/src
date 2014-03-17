#!/bin/sh
#
# $OpenBSD: signify.sh,v 1.6 2014/03/17 02:49:02 tedu Exp $

srcdir=$1

pubkey="$srcdir/regresskey.pub"
seckey="$srcdir/regresskey.sec"
orders="$srcdir/orders.txt"
forgery="$srcdir/forgery.txt"

set -e

cat $seckey | signify -S -s - -x test.sig -m $orders 
diff -u "$orders.sig" test.sig

signify -V -p $pubkey -m $orders

signify -V -p $pubkey -m $forgery 2> /dev/null && exit 1

signify -S -s $seckey -x confirmorders.sig -e -m $orders 
signify -V -p $pubkey -e -m confirmorders
diff -u $orders confirmorders

sha256 $pubkey $seckey > HASH
sha512 $orders $forgery >> HASH
signify -S -e -s $seckey -m HASH
rm HASH
signify -q -C -p $pubkey -x HASH.sig

true
