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

# $ISC: setup.sh,v 1.6 2001/01/09 21:44:04 bwelling Exp $

#
# jnl and database files MUST be removed before we start
#

rm -f ns1/*.jnl ns1/example.db ns2/*.jnl ns2/example.bk

cp ns1/example1.db ns1/example.db

# update_test.pl has its own zone file because it
# requires a specific NS record set.
cat <<\EOF >ns1/update.db
$ORIGIN .
$TTL 300        ; 5 minutes
update.nil              IN SOA  ns1.example.nil. hostmaster.example.nil. (
                                1          ; serial
                                2000       ; refresh (2000 seconds)
                                2000       ; retry (2000 seconds)
                                1814400    ; expire (3 weeks)
                                3600       ; minimum (1 hour)
                                )
update.nil.             NS      ns1.update.nil.
ns1.update.nil.         A       10.53.0.2
EOF
