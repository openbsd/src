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

# $ISC: tests.sh,v 1.7 2001/01/09 21:45:21 bwelling Exp $ 

# ns1 = stealth master
# ns2 = slave with update forwarding disabled; not currently used
# ns3 = slave with update forwarding enabled

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

echo "I:fetching master copy of zone before update"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd example.\
	@10.53.0.1 axfr -p 5300 > dig.out.ns1 || status=1

echo "I:fetching slave 1 copy of zone before update"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd example.\
	@10.53.0.2 axfr -p 5300 > dig.out.ns2 || status=1

echo "I:fetching slave 2 copy of zone before update"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd example.\
	@10.53.0.3 axfr -p 5300 > dig.out.ns3 || status=1

echo "I:comparing pre-update copies to known good data"
$PERL ../digcomp.pl knowngood.before dig.out.ns1 || status=1
$PERL ../digcomp.pl knowngood.before dig.out.ns2 || status=1
$PERL ../digcomp.pl knowngood.before dig.out.ns3 || status=1

echo "I:updating zone (signed)"
$NSUPDATE -y update.example:c3Ryb25nIGVub3VnaCBmb3IgYSBtYW4gYnV0IG1hZGUgZm9yIGEgd29tYW4K -- - <<EOF || status=1
server 10.53.0.3 5300
update add updated.example. 600 A 10.10.10.1
update add updated.example. 600 TXT Foo
send
EOF

echo "I:sleeping 15 seconds for server to incorporate changes"
sleep 15

echo "I:fetching master copy of zone after update"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd example.\
	@10.53.0.1 axfr -p 5300 > dig.out.ns1 || status=1

echo "I:fetching slave 1 copy of zone after update"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd example.\
	@10.53.0.2 axfr -p 5300 > dig.out.ns2 || status=1

echo "I:fetching slave 2 copy of zone after update"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd example.\
	@10.53.0.3 axfr -p 5300 > dig.out.ns3 || status=1

echo "I:comparing post-update copies to known good data"
$PERL ../digcomp.pl knowngood.after1 dig.out.ns1 || status=1
$PERL ../digcomp.pl knowngood.after1 dig.out.ns2 || status=1
$PERL ../digcomp.pl knowngood.after1 dig.out.ns3 || status=1

echo "I:updating zone (unsigned)"
$NSUPDATE -- - <<EOF || status=1
server 10.53.0.3 5300
update add unsigned.example. 600 A 10.10.10.1
update add unsigned.example. 600 TXT Foo
send
EOF

echo "I:sleeping 15 seconds for server to incorporate changes"
sleep 15

echo "I:fetching master copy of zone after update"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd example.\
	@10.53.0.1 axfr -p 5300 > dig.out.ns1 || status=1

echo "I:fetching slave 1 copy of zone after update"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd example.\
	@10.53.0.2 axfr -p 5300 > dig.out.ns2 || status=1

echo "I:fetching slave 2 copy of zone after update"
$DIG +tcp +noadd +nosea +nostat +noquest +nocomm +nocmd example.\
	@10.53.0.3 axfr -p 5300 > dig.out.ns3 || status=1

echo "I:comparing post-update copies to known good data"
$PERL ../digcomp.pl knowngood.after2 dig.out.ns1 || status=1
$PERL ../digcomp.pl knowngood.after2 dig.out.ns2 || status=1
$PERL ../digcomp.pl knowngood.after2 dig.out.ns3 || status=1

echo "I:exit status: $status"
exit $status
