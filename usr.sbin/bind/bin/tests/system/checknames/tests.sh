#!/bin/sh
#
# Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
# OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

# $ISC: tests.sh,v 1.2.2.2 2004/03/06 10:21:50 marka Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

status=0
n=1

DIGOPTS="+tcp +noadd +nosea +nostat +nocmd -p 5300"

# Entry should exist.
echo "I: check for failure from on zone load for 'check-names fail;' ($n)"
ret=0
$DIG $DIGOPTS fail.example. @10.53.0.1 a > dig.out.ns1.test$n || ret=1
grep SERVFAIL dig.out.ns1.test$n > /dev/null || ret=1
grep 'xx_xx.fail.example: bad owner name (check-names)' ns1/named.run > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
n=`expr $n + 1`

# Entry should exist.
echo "I: check for warnings from on zone load for 'check-names warn;' ($n)"
ret=0
grep 'xx_xx.warn.example: bad owner name (check-names)' ns1/named.run > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
n=`expr $n + 1`

# Entry should not exist.
echo "I: check for warnings from on zone load for 'check-names ignore;' ($n)"
ret=1
grep 'yy_yy.ignore.example: bad owner name (check-names)' ns1/named.run || ret=0
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
n=`expr $n + 1`

# Entry should exist
echo "I: check that 'check-names response warn;' works ($n)"
ret=0
$DIG $DIGOPTS yy_yy.ignore.example. @10.53.0.1 a > dig.out.ns1.test$n || ret=1
$DIG $DIGOPTS yy_yy.ignore.example. @10.53.0.2 a > dig.out.ns2.test$n || ret=1
$PERL ../digcomp.pl dig.out.ns1.test$n dig.out.ns2.test$n || ret=1
grep "check-names warning yy_yy.ignore.example/A/IN" ns2/named.run > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
n=`expr $n + 1`

# Entry should exist
echo "I: check that 'check-names response (owner) fails;' works ($n)"
ret=0
$DIG $DIGOPTS yy_yy.ignore.example. @10.53.0.1 a > dig.out.ns1.test$n || ret=1
$DIG $DIGOPTS yy_yy.ignore.example. @10.53.0.3 a > dig.out.ns3.test$n || ret=1
grep NOERROR dig.out.ns1.test$n > /dev/null || ret=1
grep REFUSED dig.out.ns3.test$n > /dev/null || ret=1
grep "check-names failure yy_yy.ignore.example/A/IN" ns3/named.run > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
n=`expr $n + 1`

# Entry should exist
echo "I: check that 'check-names response (rdata) fails;' works ($n)"
ret=0
$DIG $DIGOPTS mx.ignore.example. @10.53.0.1 MX > dig.out.ns1.test$n || ret=1
$DIG $DIGOPTS mx.ignore.example. @10.53.0.3 MX > dig.out.ns3.test$n || ret=1
grep NOERROR dig.out.ns1.test$n > /dev/null || ret=1
grep SERVFAIL dig.out.ns3.test$n > /dev/null || ret=1
grep "check-names failure mx.ignore.example/MX/IN" ns3/named.run > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
n=`expr $n + 1`

echo "I: check that updates to 'check-names fail;' are rejected ($n)"
ret=0
not=1
$NSUPDATE -d <<END> nsupdate.out.test$n 2>&1 || not=0
server 10.53.0.1 5300
update add xxx_xxx.fail.update. 600 A 10.10.10.1
send
END
if [ $not != 0 ]; then ret=1; fi
$DIG $DIGOPTS xxx_xxx.fail.update @10.53.0.1 A > dig.out.ns1.test$n || ret=1
grep "xxx_xxx.fail.update/A: bad owner name (check-names)" ns1/named.run > /dev/null || ret=1
grep NXDOMAIN dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
n=`expr $n + 1`

echo "I: check that updates to 'check-names warn;' succeed and are logged ($n)"
ret=0
$NSUPDATE -d <<END> nsupdate.out.test$n  2>&1|| ret=1
server 10.53.0.1 5300
update add xxx_xxx.warn.update. 600 A 10.10.10.1
send
END
$DIG $DIGOPTS xxx_xxx.warn.update @10.53.0.1 A > dig.out.ns1.test$n || ret=1
grep "xxx_xxx.warn.update/A: bad owner name (check-names)" ns1/named.run > /dev/null || ret=1
grep NOERROR dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
n=`expr $n + 1`

echo "I: check that updates to 'check-names ignore;' succeed and are not logged ($n)"
ret=0
not=1
$NSUPDATE -d <<END> nsupdate.out.test$n 2>&1 || ret=1
server 10.53.0.1 5300
update add xxx_xxx.ignore.update. 600 A 10.10.10.1
send
END
grep "xxx_xxx.ignore.update/A.*(check-names)" ns1/named.run > /dev/null || not=0
if [ $not != 0 ]; then ret=1; fi
$DIG $DIGOPTS xxx_xxx.ignore.update @10.53.0.1 A > dig.out.ns1.test$n || ret=1
grep NOERROR dig.out.ns1.test$n > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`
n=`expr $n + 1`

exit $status
