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

# $ISC: tests.sh,v 1.26 2001/01/17 20:53:44 bwelling Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

echo "I:fetching a.example from ns2's initial configuration"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd +noauth \
	a.example. @10.53.0.2 any -p 5300 > dig.out.ns2.1 || status=1
grep ";" dig.out.ns2.1	# XXXDCL why is this here?

echo "I:fetching a.example from ns3's initial configuration"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd +noauth \
	a.example. @10.53.0.3 any -p 5300 > dig.out.ns3.1 || status=1
grep ";" dig.out.ns3.1	# XXXDCL why is this here?

echo "I:copying in new configurations for ns2 and ns3"
rm -f ns2/named.conf ns3/named.conf ns2/example.db
cp ns2/named2.conf ns2/named.conf
cp ns3/named2.conf ns3/named.conf
cp ns2/example2.db ns2/example.db

echo "I:reloading ns2 and ns3 with rndc"
$RNDC -c ../common/rndc.conf -s 10.53.0.2 -p 9953 reload 2>&1 | sed 's/^/I:ns2 /'
$RNDC -c ../common/rndc.conf -s 10.53.0.3 -p 9953 reload 2>&1 | sed 's/^/I:ns3 /'

echo "I:sleeping for 20 seconds"
sleep 20

echo "I:fetching a.example from ns2's 10.53.0.4, source address 10.53.0.4"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd +noauth \
	-b 10.53.0.4 a.example. @10.53.0.4 any -p 5300 > dig.out.ns4.2 \
	|| status=1
grep ";" dig.out.ns4.2	# XXXDCL why is this here?

echo "I:fetching a.example from ns2's 10.53.0.2, source address 10.53.0.2"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd +noauth \
	-b 10.53.0.2 a.example. @10.53.0.2 any -p 5300 > dig.out.ns2.2 \
	|| status=1
grep ";" dig.out.ns2.2	# XXXDCL why is this here?

echo "I:fetching a.example from ns3's 10.53.0.3, source address defaulted"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd +noauth \
	@10.53.0.3 a.example. any -p 5300 > dig.out.ns3.2 || status=1
grep ";" dig.out.ns3.2	# XXXDCL why is this here?

echo "I:comparing ns3's initial a.example to one from reconfigured 10.53.0.2"
$PERL ../digcomp.pl dig.out.ns3.1 dig.out.ns2.2 || status=1

echo "I:comparing ns3's initial a.example to one from reconfigured 10.53.0.3"
$PERL ../digcomp.pl dig.out.ns3.1 dig.out.ns3.2 || status=1

echo "I:comparing ns2's initial a.example to one from reconfigured 10.53.0.4"
$PERL ../digcomp.pl dig.out.ns2.1 dig.out.ns4.2 || status=1

echo "I:comparing ns2's initial a.example to one from reconfigured 10.53.0.3"
echo "I:(should be different)"
if $PERL ../digcomp.pl dig.out.ns2.1 dig.out.ns3.2 >/dev/null
then
	echo "I:no differences found.  something's wrong."
	status=1
fi

echo "I:exit status: $status"
exit $status
