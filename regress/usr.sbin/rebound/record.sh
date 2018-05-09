#!/bin/sh
#
# $OpenBSD: record.sh,v 1.1 2018/05/09 19:34:53 anton Exp $
#
# Copyright (c) 2018 Anton Lindqvist <anton@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

cat <<! >$CONF
nameserver ${ADDR1}
# Unknown type.
record MX t1.example.com. 1.1.1.1
# Missing trailing dot.
record A t1.example.com 1.1.1.1
# Name too short.
record A a 1.1.1.1
!

$ENV $REBOUND -c $CONF -l $ADDR2

R=$(resolve -W 1 -t MX t1.example.com $ADDR2)
asserteq ";; connection timed out; no servers could be reached" "$R" \
	"must not be resolved by rebound"

R=$(resolve -W 1 -t A t1.example.com $ADDR2)
asserteq ";; connection timed out; no servers could be reached" "$R" \
	"must not be resolved by rebound"

R=$(resolve -W 1 -t A a $ADDR2)
asserteq ";; connection timed out; no servers could be reached" "$R" \
	"must not be resolved by rebound"

pkillw "^${REBOUND}"

cat <<! >$CONF
nameserver ${ADDR1}
record A t1.example.com. 1.1.1.1
record A t2.example.com. 2.2.2.2
!

$ENV $NS -l $ADDR1 A t3.example.com. 3.3.3.3

$ENV $REBOUND -c $CONF -l $ADDR2

R=$(resolve -t A t1.example.com $ADDR2)
asserteq "t1.example.com has address 1.1.1.1" "$R" "must be resolved by rebound"

R=$(resolve -t PTR 1.1.1.1 $ADDR2)
asserteq "1.1.1.1.in-addr.arpa domain name pointer t1.example.com." "$R" \
	"must be resolved by rebound"

R=$(resolve -t A t2.example.com $ADDR2)
asserteq "t2.example.com has address 2.2.2.2" "$R" "must be resolved by rebound"

R=$(resolve -t PTR 2.2.2.2 $ADDR2)
asserteq "2.2.2.2.in-addr.arpa domain name pointer t2.example.com." "$R" \
	"must be resolved by rebound"

R=$(resolve -t A t3.example.com $ADDR2)
asserteq "t3.example.com has address 3.3.3.3" "$R" \
	"must be resolved by rebound-ns"

R=$(resolve -t A t4.example.com $ADDR2)
asserteq "Host t4.example.com not found: 3(NXDOMAIN)" "$R" \
	"must not be resolved by rebound nor rebound-ns"
