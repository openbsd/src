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

# $ISC: sign.sh,v 1.2.4.1 2004/05/14 05:20:45 marka Exp $

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

RANDFILE=../random.data
dlvsets=

zone=child1.utld.
infile=child.db.in
zonefile=child1.utld.db
outfile=child1.signed
dlvzone=dlv.utld.
dlvsets="$dlvsets dlvset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -g -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null 
echo "I: signed $zone"


zone=child3.utld.
infile=child.db.in
zonefile=child3.utld.db
outfile=child3.signed
dlvzone=dlv.utld.
dlvsets="$dlvsets dlvset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -g -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null
echo "I: signed $zone"


zone=child4.utld.
infile=child.db.in
zonefile=child4.utld.db
outfile=child4.signed
dlvzone=dlv.utld.
dlvsets="$dlvsets dlvset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -g -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null
echo "I: signed $zone"


zone=child5.utld.
infile=child.db.in
zonefile=child5.utld.db
outfile=child5.signed
dlvzone=dlv.utld.
dlvsets="$dlvsets dlvset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -g -r $RANDFILE -o $zone -f $outfile $zonefile > /dev/null
echo "I: signed $zone"


zone=child7.utld.
infile=child.db.in
zonefile=child7.utld.db
outfile=child7.signed
dlvzone=dlv.utld.

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -g -r $RANDFILE -o $zone -f $outfile $zonefile > /dev/null
echo "I: signed $zone"


zone=child8.utld.
infile=child.db.in
zonefile=child8.utld.db
outfile=child8.signed
dlvzone=dlv.utld.

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -g -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null
echo "I: signed $zone"


zone=child9.utld.
infile=child.db.in
zonefile=child9.utld.db
outfile=child9.signed
dlvzone=dlv.utld.
dlvsets="$dlvsets dlvset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -g -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null
echo "I: signed $zone"

zone=child10.utld.
infile=child.db.in
zonefile=child10.utld.db
outfile=child10.signed
dlvzone=dlv.utld.
dlvsets="$dlvsets dlvset-$zone"

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -g -r $RANDFILE -l $dlvzone -o $zone -f $outfile $zonefile > /dev/null
echo "I: signed $zone"


zone=dlv.utld.
infile=dlv.db.in
zonefile=dlv.utld.db
outfile=dlv.signed
dlvzone=dlv.utld.

keyname1=`$KEYGEN -r $RANDFILE -a DSA -b 768 -n zone $zone`
keyname2=`$KEYGEN -f KSK -r $RANDFILE -a DSA -b 768 -n zone $zone`

cat $infile $dlvsets $keyname1.key $keyname2.key >$zonefile

$SIGNER -g -r $RANDFILE -o $zone -f $outfile $zonefile > /dev/null
echo "I: signed $zone"


cat $keyname2.key | $PERL -n -e '
local ($dn, $class, $type, $flags, $proto, $alg, @rest) = split;
local $key = join("", @rest);
print <<EOF
trusted-keys {
    "$dn" $flags $proto $alg "$key";
};
EOF
' > trusted.conf
cp trusted.conf ../ns5
