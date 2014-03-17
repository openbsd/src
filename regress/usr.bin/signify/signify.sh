#!/bin/sh
#
# $OpenBSD: signify.sh,v 1.5 2014/03/17 02:43:15 tedu Exp $

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

true
