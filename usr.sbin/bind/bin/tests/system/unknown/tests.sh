#!/bin/sh
#
# Copyright (C) 2000, 2001  Internet Software Consortium.
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

# $ISC: tests.sh,v 1.7 2001/01/09 21:45:07 bwelling Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

DIGOPTS="@10.53.0.1 -p 5300"

echo "I:querying for various representations of an IN A record"
for i in 1 2 3 4 5 6 7 8 9 10 11 12
do
	ret=0
	$DIG +short $DIGOPTS a$i.example a in > dig.out || ret=1
	echo 10.0.0.1 | diff - dig.out || ret=1
	if [ $ret != 0 ]
	then
		echo "#$i failed"
	fi
	status=`expr $status + $ret`
done

echo "I:querying for various representations of an IN TXT record"
for i in 1 2 3 4 5 6 7
do
	ret=0
	$DIG +short $DIGOPTS txt$i.example txt in > dig.out || ret=1
	echo '"hello"' | diff - dig.out || ret=1
	if [ $ret != 0 ]
	then
		echo "#$i failed"
	fi
	status=`expr $status + $ret`
done

echo "I:querying for various representations of an IN TYPE123 record"
for i in 1 2 3
do
	ret=0
	$DIG +short $DIGOPTS unk$i.example type123 in > dig.out || ret=1
	echo '\# 1 00' | diff - dig.out || ret=1
	if [ $ret != 0 ]
	then
		echo "#$i failed"
	fi
	status=`expr $status + $ret`
done

echo "I:querying for various representations of a CLASS10 TYPE1 record"
for i in 1 2
do
	ret=0
	$DIG +short $DIGOPTS a$i.example a class10 > dig.out || ret=1
	echo '\# 4 0A000001' | diff - dig.out || ret=1
	if [ $ret != 0 ]
	then
		echo "#$i failed"
	fi
	status=`expr $status + $ret`
done

echo "I:querying for various representations of a CLASS10 TXT record"
for i in 1 2 3 4
do
	ret=0
	$DIG +short $DIGOPTS txt$i.example txt class10 > dig.out || ret=1
	echo '"hello"' | diff - dig.out || ret=1
	if [ $ret != 0 ]
	then
		echo "#$i failed"
	fi
	status=`expr $status + $ret`
done

echo "I:querying for various representations of a CLASS10 TYPE123 record"
for i in 1 2
do
	ret=0
	$DIG +short $DIGOPTS unk$i.example type123 class10 > dig.out || ret=1
	echo '\# 1 00' | diff - dig.out || ret=1
	if [ $ret != 0 ]
	then
		echo "#$i failed"
	fi
	status=`expr $status + $ret`
done

echo "I:querying for SOAs of zone that should have failed to load"
for i in 1 2 3 4
do
	ret=0
	$DIG $DIGOPTS broken$i. soa in > dig.out || ret=1
	grep "SERVFAIL" dig.out > /dev/null || ret=1
	if [ $ret != 0 ]
	then
		echo "#$i failed"
	fi
	status=`expr $status + $ret`
done

echo "I:exit status: $status"
exit $status
