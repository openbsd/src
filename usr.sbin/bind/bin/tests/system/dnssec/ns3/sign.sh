#!/bin/sh
#
# Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
# Copyright (C) 2000-2002  Internet Software Consortium.
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

# $ISC: sign.sh,v 1.12.12.3 2004/03/08 04:04:35 marka Exp $

RANDFILE=../random.data

zone=secure.example.
infile=secure.example.db.in
zonefile=secure.example.db

keyname=`$KEYGEN -r $RANDFILE -a RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -r $RANDFILE -o $zone $zonefile > /dev/null

zone=bogus.example.
infile=bogus.example.db.in
zonefile=bogus.example.db

keyname=`$KEYGEN -r $RANDFILE -a RSA -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -r $RANDFILE -o $zone $zonefile > /dev/null

zone=dynamic.example.
infile=dynamic.example.db.in
zonefile=dynamic.example.db

keyname=`$KEYGEN -r $RANDFILE -a RSA -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -r $RANDFILE -o $zone $zonefile > /dev/null

zone=keyless.example.
infile=keyless.example.db.in
zonefile=keyless.example.db

keyname=`$KEYGEN -r $RANDFILE -a RSA -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -r $RANDFILE -o $zone $zonefile > /dev/null

# Change the signer field of the a.b.keyless.example SIG A
# to point to a provably nonexistent KEY record.
mv $zonefile.signed $zonefile.tmp
<$zonefile.tmp perl -p -e 's/ keyless.example/ b.keyless.example/
    if /^a.b.keyless.example/../NXT/;' >$zonefile.signed
rm -f $zonefile.tmp
