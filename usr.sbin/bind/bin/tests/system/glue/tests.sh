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

# $ISC: tests.sh,v 1.5 2001/01/09 21:43:18 bwelling Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

#
# Do glue tests.
#

status=0

echo "I:testing that a ccTLD referral gets a full glue set from the root zone"
$DIG +norec @10.53.0.1 -p 5300 foo.bar.fi. A >dig.out || status=1
$PERL ../digcomp.pl fi.good dig.out || status=1

echo "I:testing that we find glue A RRs we are authoritative for"
$DIG +norec @10.53.0.1 -p 5300 foo.bar.xx. a >dig.out || status=1
$PERL ../digcomp.pl xx.good dig.out || status=1

echo "I:testing that we find glue A/AAAA/A6 RRs in the cache"
$DIG +norec @10.53.0.1 -p 5300 foo.bar.yy. a >dig.out || status=1
$PERL ../digcomp.pl yy.good dig.out || status=1

echo "I:testing that we don't find out-of-zone glue"
$DIG +norec @10.53.0.1 -p 5300 example.net. a > dig.out || status=1
$PERL ../digcomp.pl noglue.good dig.out || status=1

echo "I:exit status: $status"
exit $status
