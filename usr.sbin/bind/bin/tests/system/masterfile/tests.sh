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

# $ISC: tests.sh,v 1.2 2001/08/09 00:10:56 gson Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

echo "I:test master file \$INCLUDE semantics"
$DIG +nostats +nocmd include. axfr @10.53.0.1 -p 5300 >dig.out

echo "I:test master file BIND 8 compatibility TTL and \$TTL semantics"
$DIG +nostats +nocmd ttl2. axfr @10.53.0.1 -p 5300 >>dig.out

echo "I:test of master file RFC1035 TTL and \$TTL semantics"
$DIG +nostats +nocmd ttl2. axfr @10.53.0.1 -p 5300 >>dig.out

diff dig.out knowngood.dig.out || status=1

echo "I:exit status: $status"
exit $status
