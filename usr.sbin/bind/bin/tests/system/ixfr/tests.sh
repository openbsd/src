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

# $ISC: tests.sh,v 1.2 2001/05/10 19:05:00 gson Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

DIGOPTS="+tcp +noadd +nosea +nostat +noquest +nocomm +nocmd"
DIGCMD="$DIG $DIGOPTS @10.53.0.1 -p 5300"
SENDCMD="$PERL ../send.pl 10.53.0.2 5301"
RNDCCMD="$RNDC -s 10.53.0.1 -p 9953 -c ../common/rndc.conf"

echo "I:testing initial AXFR"

$SENDCMD <<EOF
/SOA/
nil.      	300	SOA	ns.nil. root.nil. 1 300 300 604800 300
/AXFR/
nil.      	300	SOA	ns.nil. root.nil. 1 300 300 604800 300
nil.      	300	NS	ns.nil.
nil.		300	TXT	"initial AXFR"
a.nil.		60	A	10.0.0.61
b.nil.		60	A	10.0.0.62
nil.      	300	SOA	ns.nil. root.nil. 1 300 300 604800 300
EOF

sleep 1

# Initially, ns1 is not authoritative for anything (see setup.sh).
# Now that ans is up and running with the right data, we make it
# a slave for nil.

cat <<EOF >>ns1/named.conf
zone "nil" {
	type slave;
	file "myftp.db";
	masters { 10.53.0.2; };
};
EOF

$RNDCCMD reload

sleep 2

$DIGCMD nil. TXT | grep 'initial AXFR' >/dev/null || {
    echo "I:failed"
    status=1
}

echo "I:testing successful IXFR"

# We change the IP address of a.nil., and the TXT record at the apex.
# Then we do a SOA-only update.

$SENDCMD <<EOF
/SOA/
nil.      	300	SOA	ns.nil. root.nil. 3 300 300 604800 300
/IXFR/
nil.      	300	SOA	ns.nil. root.nil. 3 300 300 604800 300
nil.      	300	SOA	ns.nil. root.nil. 1 300 300 604800 300
a.nil.      	60	A	10.0.0.61
nil.		300	TXT	"initial AXFR"
nil.      	300	SOA	ns.nil. root.nil. 2 300 300 604800 300
nil.		300	TXT	"successful IXFR"
a.nil.      	60	A	10.0.1.61
nil.      	300	SOA	ns.nil. root.nil. 2 300 300 604800 300
nil.      	300	SOA	ns.nil. root.nil. 3 300 300 604800 300
nil.      	300	SOA	ns.nil. root.nil. 3 300 300 604800 300
EOF

sleep 1

$RNDCCMD refresh nil

sleep 2

$DIGCMD nil. TXT | grep 'successful IXFR' >/dev/null || {
    echo "I:failed"
    status=1
}

echo "I:testing AXFR fallback after IXFR failure"

# Provide a broken IXFR response and a working fallback AXFR response

$SENDCMD <<EOF
/SOA/
nil.      	300	SOA	ns.nil. root.nil. 4 300 300 604800 300
/IXFR/
nil.      	300	SOA	ns.nil. root.nil. 4 300 300 604800 300
nil.      	300	SOA	ns.nil. root.nil. 3 300 300 604800 300
nil.      	300	TXT	"delete-nonexistent-txt-record"
nil.      	300	SOA	ns.nil. root.nil. 4 300 300 604800 300
nil.      	300	TXT	"this-txt-record-would-be-added"
nil.      	300	SOA	ns.nil. root.nil. 4 300 300 604800 300
/AXFR/
nil.      	300	SOA	ns.nil. root.nil. 3 300 300 604800 300
nil.      	300	NS	ns.nil.
nil.      	300	TXT	"fallback AXFR"
nil.      	300	SOA	ns.nil. root.nil. 3 300 300 604800 300
EOF

sleep 1

$RNDCCMD refresh nil

sleep 2

$DIGCMD nil. TXT | grep 'fallback AXFR' >/dev/null || {
    echo "I:failed"
    status=1
}

echo "I:exit status: $status"
exit $status
