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

# $ISC: tests.sh,v 1.4 2001/03/09 18:49:52 bwelling Exp $

SYSTEMTESTTOP=..
. $SYSTEMTESTTOP/conf.sh

root=10.53.0.1
hidden=10.53.0.2
f1=10.53.0.3
f2=10.53.0.4

status=0

echo "I:checking that a forward zone overrides global forwarders"
ret=0
$DIG txt.example1. txt @$hidden -p 5300 > dig.out.hidden || ret=1
$DIG txt.example1. txt @$f1 -p 5300 > dig.out.f1 || ret=1
$PERL ../digcomp.pl dig.out.hidden dig.out.f1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that a forward first zone no forwarders recurses"
ret=0
$DIG txt.example2. txt @$root -p 5300 > dig.out.root || ret=1
$DIG txt.example2. txt @$f1 -p 5300 > dig.out.f1 || ret=1
$PERL ../digcomp.pl dig.out.root dig.out.f1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that a forward only zone no forwarders fails"
ret=0
$DIG txt.example2. txt @$root -p 5300 > dig.out.root || ret=1
$DIG txt.example2. txt @$f1 -p 5300 > dig.out.f1 || ret=1
$PERL ../digcomp.pl dig.out.root dig.out.f1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that global forwarders work"
ret=0
$DIG txt.example4. txt @$hidden -p 5300 > dig.out.hidden || ret=1
$DIG txt.example4. txt @$f1 -p 5300 > dig.out.f1 || ret=1
$PERL ../digcomp.pl dig.out.hidden dig.out.f1 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that a forward zone works"
ret=0
$DIG txt.example1. txt @$hidden -p 5300 > dig.out.hidden || ret=1
$DIG txt.example1. txt @$f2 -p 5300 > dig.out.f2 || ret=1
$PERL ../digcomp.pl dig.out.hidden dig.out.f2 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that forwarding doesn't spontaneously happen"
ret=0
$DIG txt.example2. txt @$root -p 5300 > dig.out.root || ret=1
$DIG txt.example2. txt @$f2 -p 5300 > dig.out.f2 || ret=1
$PERL ../digcomp.pl dig.out.root dig.out.f2 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that a forward zone with no specified policy works"
ret=0
$DIG txt.example3. txt @$hidden -p 5300 > dig.out.hidden || ret=1
$DIG txt.example3. txt @$f2 -p 5300 > dig.out.f2 || ret=1
$PERL ../digcomp.pl dig.out.hidden dig.out.f2 || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:checking that a forward only doesn't recurse"
ret=0
$DIG txt.example5. txt @$f2 -p 5300 > dig.out.f2 || ret=1
grep "SERVFAIL" dig.out.f2 > /dev/null || ret=1
if [ $ret != 0 ]; then echo "I:failed"; fi
status=`expr $status + $ret`

echo "I:exit status: $status"
exit $status
