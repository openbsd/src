#!/usr/bin/perl
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

# $ISC: setup.pl,v 1.10 2001/01/09 21:45:57 bwelling Exp $

#
# Set up test data for zone transfer quota tests.
#
use FileHandle;

my $masterconf = new FileHandle("ns1/zones.conf", "w") or die;
my $slaveconf  = new FileHandle("ns2/zones.conf", "w") or die;

for ($z = 0; $z < 300; $z++) {
    my $zn = sprintf("zone%06d.example", $z);
    print $masterconf "zone \"$zn\" { type master; file \"$zn.db\"; };\n";
    print $slaveconf  "zone \"$zn\" { type slave; file \"$zn.bk\"; masters { 10.53.0.1; }; };\n";
    my $fn = "ns1/$zn.db";
    my $f = new FileHandle($fn, "w") or die "open: $fn: $!";
    print $f "\$TTL 300
\@	IN SOA 	. . 1 300 120 3600 86400
	NS	ns1
	NS	ns2
	MX	10 mail1.isp.example.
	MX	20 mail2.isp.example.
www	A	10.0.0.1
xyzzy   A       10.0.0.2
";
    $f->close;
}
