#!/bin/sh
#
# $OpenBSD: signify.sh,v 1.1 2014/01/09 16:13:44 tedu Exp $

srcdir=$1

pubkey="$srcdir/regresskey.pub"
seckey="$srcdir/regresskey.sec"
orders="$srcdir/orders.txt"
forgery="$srcdir/forgery.txt"

set -e

signify -p $pubkey -V $orders > /dev/null
signify -p $pubkey -V $forgery 2> /dev/null && exit 1

true
