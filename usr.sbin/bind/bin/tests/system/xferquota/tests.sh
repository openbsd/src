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

# $ISC: tests.sh,v 1.20 2001/01/09 21:45:59 bwelling Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

#
# Perform tests
#

count=0
ticks=0
while [ $count != 300 ]; do
        if [ $ticks = 1 ]; then
	        echo "I:Changing test zone..."
		cp ns1/changing2.db ns1/changing.db
		kill -HUP `cat ns1/named.pid`
	fi
	sleep 1
	ticks=`expr $ticks + 1`
	seconds=`expr $ticks \* 1`
	if [ $ticks = 360 ]; then
		echo "I:Took too long to load zones"
		exit 1
	fi
	count=`cat ns2/zone*.bk | grep xyzzy | wc -l`
	echo "I:Have $count zones up in $seconds seconds"
done

status=0

$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd \
	zone000099.example. @10.53.0.1 axfr -p 5300 > dig.out.ns1 || status=1
grep ";" dig.out.ns1

$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd \
	zone000099.example. @10.53.0.2 axfr -p 5300 > dig.out.ns2 || status=1
grep ";" dig.out.ns2

$PERL ../digcomp.pl dig.out.ns1 dig.out.ns2 || status=1

sleep 5

$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd \
	a.changing. @10.53.0.1 a -p 5300 > dig.out.ns1 || status=1
grep ";" dig.out.ns1

$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd \
	a.changing. @10.53.0.2 a -p 5300 > dig.out.ns2 || status=1
grep ";" dig.out.ns2

$PERL ../digcomp.pl dig.out.ns1 dig.out.ns2 || status=1

echo "I:exit status: $status"
exit $status
