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

# $ISC: tests.sh,v 1.1 2001/01/10 01:18:27 gson Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

# ns1 = root server
# ns2 = authoritative server
# ns3 = recursive server doing v6 synthesis

status=0

DIGOPTS="+tcp +noadd +nosea +nostat +noquest +nocomm +nocmd"

for name in aaaa a6 chain alias2 aaaa.dname loop loop2
do
    $DIG $DIGOPTS $name.example. aaaa @10.53.0.3 -p 5300
    echo
done >dig.out

for i in 1 2
do
    $DIG $DIGOPTS f.f.$i.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.6.5.4.3.2.1.ip6.int. PTR @10.53.0.3 -p 5300
    echo
done >>dig.out

cat <<EOF >good.out
aaaa.example.		0	IN	AAAA	12:34:56::ff

a6.example.		0	IN	AAAA	12:34:56::ff

chain.example.		0	IN	AAAA	12:34:56::ff:ff

alias2.example.		0	IN	CNAME	alias.example.
alias.example.		0	IN	CNAME	chain.example.
chain.example.		0	IN	AAAA	12:34:56::ff:ff

aaaa.dname.example.	0	IN	CNAME	aaaa.foo.example.
aaaa.foo.example.	0	IN	AAAA	12:34:56::ff

loop.example.		0	IN	CNAME	loop.example.

loop2.example.		0	IN	CNAME	loop3.example.
loop3.example.		0	IN	CNAME	loop2.example.

f.f.1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.6.5.4.3.2.1.ip6.int. 0 IN PTR foo.

f.f.2.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.6.5.4.3.2.1.ip6.int. 0 IN PTR bar.

EOF

diff good.out dig.out || status=1

echo "I:exit status: $status"
exit $status
