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

# $ISC: tests.sh,v 1.3 2001/01/09 21:42:32 bwelling Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

rm -f dig.out.*

DIGOPTS="+norec +tcp +noadd +nosea +nostat +noquest +nocmd -p 5300"

# Check the example. domain

$DIG $DIGOPTS example. @10.53.0.1 soa > dig.out.ns1.test || ret=1
echo "I:checking that first zone transfer worked"
ret=0
try=0
while test $try -lt 120
do
	$DIG $DIGOPTS example. @10.53.0.2 soa > dig.out.ns2.test || ret=1
	if grep SERVFAIL dig.out.ns2.test > /dev/null
	then
		try=`expr $try + 1`
		sleep 1
	else
		$PERL ../digcomp.pl dig.out.ns1.test dig.out.ns2.test || ret=1
		break;
	fi
done
echo "I:try $try"
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that second zone transfer worked"
ret=0
try=0
while test $try -lt 120
do
	$DIG $DIGOPTS example. @10.53.0.3 soa > dig.out.ns3.test || ret=1
	if grep SERVFAIL dig.out.ns3.test > /dev/null
	then
		try=`expr $try + 1`
		sleep 1
	else
		$PERL ../digcomp.pl dig.out.ns1.test dig.out.ns3.test || ret=1
		break;
	fi
done
echo "I:try $try"
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
