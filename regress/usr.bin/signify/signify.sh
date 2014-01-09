#!/bin/sh
#
# $OpenBSD: signify.sh,v 1.3 2014/01/09 18:59:56 tedu Exp $

srcdir=$1

pubkey="$srcdir/regresskey.pub"
seckey="$srcdir/regresskey.sec"
orders="$srcdir/orders.txt"
forgery="$srcdir/forgery.txt"

set -e

signify -s $seckey -o test.sig -S $orders 
diff -u test.sig "$orders.sig"
rm test.sig

signify -p $pubkey -V $orders > /dev/null

signify -p $pubkey -V $forgery 2> /dev/null && exit 1

true
