#!/bin/sh
#
# $OpenBSD: reload.sh,v 1.1 2018/05/09 19:34:53 anton Exp $
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

echo nameserver $ADDR1 >$CONF

$ENV $NS -l $ADDR1 A t1.example.com. 1.1.1.1

$ENV $REBOUND -c $CONF -l $ADDR2

R=$(resolve -t A t1.example.com $ADDR2)
asserteq "t1.example.com has address 1.1.1.1" "$R" \
	"must be resolved by rebound-ns"

# Kill and restart rebound-ns in order to bind to a different address.
pkillw "^${NS}"
echo nameserver $ADDR3 >$CONF
$ENV $NS -l $ADDR3 A t2.example.com. 2.2.2.2 A t3.example.com. 3.3.3.3

R=$(resolve -t A t2.example.com $ADDR2)
asserteq "t2.example.com has address 2.2.2.2" "$R" \
	"must be resolved by rebound-ns"

# It should survive a SIGHUP delivery.
pkill -HUP -f "^${REBOUND}"

R=$(resolve -t A t3.example.com $ADDR2)
asserteq "t3.example.com has address 3.3.3.3" "$R" \
	"must be resolved by rebound-ns"

# Clear resolv.conf, rebound should exit since all nameservers are gone.
echo >$CONF
NTRIES=0
while pgrep -f "^${REBOUND}" >/dev/null 2>&1; do
	sleep .1
	NTRIES=$((NTRIES + 1))
	[ $NTRIES -eq 100 ] && break
done
R=$(pgrep -fl "^${REBOUND}" | xargs)
asserteq "" "$R" "rebound not dead after $((NTRIES / 10)) seconds"
