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

# $ISC: tests.sh,v 1.14 2001/02/14 02:03:45 gson Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0

echo "I:using resolv.conf"
ret=0
./lwtest || ret=1
if [ $ret != 0 ]; then
	echo "I:failed"
fi
status=`expr $status + $ret`

$PERL $SYSTEMTESTTOP/stop.pl . lwresd1

$PERL $SYSTEMTESTTOP/start.pl . lwresd1 -- "-c lwresd.conf -d 99 -g"

echo "I:using lwresd.conf"
ret=0
./lwtest || ret=1
if [ $ret != 0 ]; then
	echo "I:failed"
fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
