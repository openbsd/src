#!/bin/sh
#
# Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
# Copyright (C) 2000-2003  Internet Software Consortium.
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

# $ISC: sign.sh,v 1.15.12.4 2004/03/10 02:55:54 marka Exp $

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

RANDFILE=../random.data

zone=example.
infile=example.db.in
zonefile=example.db

# Have the child generate a zone key and pass it to us.

( cd ../ns3 && sh sign.sh )

for subdomain in secure bogus dynamic keyless
do
	cp ../ns3/keyset-$subdomain.example. .
done

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone`
keyname2=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -g -r $RANDFILE -o $zone -k $keyname1 $zonefile $keyname2 > /dev/null

# Sign the privately secure file

privzone=private.secure.example.
privinfile=private.secure.example.db.in
privzonefile=private.secure.example.db

privkeyname=`$KEYGEN -r $RANDFILE -a RSA -b 768 -n zone $privzone`

cat $privinfile $privkeyname.key >$privzonefile

$SIGNER -g -r $RANDFILE -o $privzone -l dlv $privzonefile > /dev/null

# Sign the DLV secure zone.


dlvzone=dlv.
dlvinfile=dlv.db.in
dlvzonefile=dlv.db

dlvkeyname=`$KEYGEN -r $RANDFILE -a RSA -b 768 -n zone $dlvzone`

cat $dlvinfile $dlvkeyname.key dlvset-$privzone > $dlvzonefile

$SIGNER -g -r $RANDFILE -o $dlvzone $dlvzonefile > /dev/null
