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

# $ISC: sign.sh,v 1.15 2001/01/09 21:42:55 bwelling Exp $

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

RANDFILE=../random.data

zone=example.
infile=example.db.in
zonefile=example.db

keyname=`$KEYGEN -r $RANDFILE -a RSA -b 768 -n zone $zone`

# Have the child generate a zone key and pass it to us,
# sign it, and pass it back

( cd ../ns3 && sh sign.sh )

cp ../ns3/keyset-secure.example. .

$KEYSIGNER -r $RANDFILE keyset-secure.example. $keyname > /dev/null

# This will leave two copies of the child's zone key in the signed db file;
# that shouldn't cause any problems.
cat signedkey-secure.example. >>../ns3/secure.example.db.signed

cp ../ns3/keyset-bogus.example. .

$KEYSIGNER -r $RANDFILE keyset-bogus.example. $keyname > /dev/null

# This will leave two copies of the child's zone key in the signed db file;
# that shouldn't cause any problems.
cat signedkey-bogus.example. >>../ns3/bogus.example.db.signed

$KEYSETTOOL -r $RANDFILE -t 3600 $keyname > /dev/null

cat $infile $keyname.key >$zonefile

$SIGNER -r $RANDFILE -o $zone $zonefile > /dev/null

# Sign the privately secure file

privzone=private.secure.example.
privinfile=private.secure.example.db.in
privzonefile=private.secure.example.db

privkeyname=`$KEYGEN -r $RANDFILE -a RSA -b 768 -n zone $privzone`

cat $privinfile $privkeyname.key >$privzonefile

$SIGNER -r $RANDFILE -o $privzone $privzonefile > /dev/null
