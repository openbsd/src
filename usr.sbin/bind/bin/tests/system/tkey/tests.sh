#!/bin/sh
#
# Copyright (C) 2001  Internet Software Consortium.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
# DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
# INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# $ISC: tests.sh,v 1.2 2001/01/11 20:51:16 bwelling Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

DIGOPTS="@10.53.0.1 -p 5300"

status=0

RANDFILE=random.data

echo "I:generating new DH key"
ret=0
dhkeyname=`$KEYGEN -a DH -b 768 -n host -r $RANDFILE client` || ret=1
if [ $ret != 0 ]; then
	echo "I:failed"
	echo "I:exit status: $status"
	exit $status
fi
status=`expr $status + $ret`

echo "I:creating new key"
ret=0
keyname=`./keycreate $dhkeyname` || ret=1
if [ $ret != 0 ]; then
	echo "I:failed"
	echo "I:exit status: $status"
	exit $status
fi
status=`expr $status + $ret`

echo "I:checking the new key"
ret=0
$DIG $DIGOPTS . ns -k $keyname > dig.out.1 || ret=1
grep "status: NOERROR" dig.out.1 > /dev/null || ret=1
grep "TSIG.*hmac-md5.*NOERROR" dig.out.1 > /dev/null || ret=1
grep "Some TSIG could not be validated" dig.out.1 > /dev/null && ret=1
if [ $ret != 0 ]; then
	echo "I:failed"
fi
status=`expr $status + $ret`

echo "I:deleting new key"
ret=0
./keydelete $keyname || ret=1
if [ $ret != 0 ]; then
	echo "I:failed"
fi
status=`expr $status + $ret`

echo "I:checking that new key has been deleted"
ret=0
$DIG $DIGOPTS . ns -k $keyname > dig.out.2 || ret=1
grep "status: NOERROR" dig.out.2 > /dev/null && ret=1
grep "TSIG.*hmac-md5.*NOERROR" dig.out.2 > /dev/null && ret=1
grep "Some TSIG could not be validated" dig.out.2 > /dev/null || ret=1
if [ $ret != 0 ]; then
	echo "I:failed"
fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
