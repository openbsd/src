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

# $ISC: tests.sh,v 1.14 2001/02/14 02:42:10 gson Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

echo "I:1000 A records"
$DIG +tcp +norec 1000.example. @10.53.0.1 a -p 5300 > dig.out.1000 || status=1
#dig 1000.example. @10.53.0.1 a -p 5300 > knowngood.dig.out.1000
$PERL ../digcomp.pl knowngood.dig.out.1000 dig.out.1000 || status=1

echo "I:2000 A records"
$DIG +tcp +norec 2000.example. @10.53.0.1 a -p 5300 > dig.out.2000 || status=1
#dig 2000.example. @10.53.0.1 a -p 5300 > knowngood.dig.out.2000
$PERL ../digcomp.pl knowngood.dig.out.2000 dig.out.2000 || status=1

echo "I:3000 A records"
$DIG +tcp +norec 3000.example. @10.53.0.1 a -p 5300 > dig.out.3000 || status=1
#dig 3000.example. @10.53.0.1 a -p 5300 > knowngood.dig.out.3000
$PERL ../digcomp.pl knowngood.dig.out.3000 dig.out.3000 || status=1

echo "I:4000 A records"
$DIG +tcp +norec 4000.example. @10.53.0.1 a -p 5300 > dig.out.4000 || status=1
#dig 4000.example. @10.53.0.1 a -p 5300 > knowngood.dig.out.4000
$PERL ../digcomp.pl knowngood.dig.out.4000 dig.out.4000 || status=1

echo "I:exactly maximum rrset"
$DIG +tcp +norec a-maximum-rrset.example. @10.53.0.1 a -p 5300 > dig.out.a-maximum-rrset \
	|| status=1
#dig a-maximum-rrset.example. @10.53.0.1 a -p 5300 > knowngood.dig.out.a-maximum-rrset
$PERL ../digcomp.pl knowngood.dig.out.a-maximum-rrset dig.out.a-maximum-rrset || status=1

echo "I:exceed maximum rrset (5000 A records)"
$DIG +tcp +norec 5000.example. @10.53.0.1 a -p 5300 > dig.out.exceed || status=1
# Look for truncation bit (tc).
grep 'flags: .*tc.*;' dig.out.exceed > /dev/null || {
    echo "I:TC bit was not set"
    status=1
}

echo "I:exit status: $status"
exit $status
