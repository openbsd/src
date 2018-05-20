#!/bin/sh
#
# $OpenBSD: localhost.sh,v 1.1 2018/05/09 19:34:53 anton Exp $
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
nameserver 127.0.0.1
nameserver ${ADDR1}
!

$ENV $NS -l $ADDR1 A t1.example.com. 1.1.1.1

$ENV $REBOUND -c $CONF -l $ADDR2

R=$(resolve -t A t1.example.com $ADDR2)
asserteq "t1.example.com has address 1.1.1.1" "$R" \
	"must be resolved using rebound-ns since localhost is skipped"
