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

# $ISC: setup.pl,v 1.2 2001/01/09 21:44:44 bwelling Exp $

#
# Set up test data for zone transfer quota tests.
#
use FileHandle;

my $n_zones = 5;
my $n_names = 1000;

make_zones(2, undef);
make_zones(3, "10.53.0.2");
make_zones(4, "10.53.0.3");

my $rootdelegations =
    new FileHandle("ns1/root.db", "w") or die;

print $rootdelegations <<END;
$TTL 300
.                       IN SOA  gson.nominum.com. a.root.servers.nil. (
								       2000042100      ; serial
								       600             ; refresh
								       600             ; retry
								       1200            ; expire
								       600             ; minimum
                                )
.                       NS      a.root-servers.nil.
a.root-servers.nil.     A       10.53.0.1
END

for ($z = 0; $z < $n_zones; $z++) {
	my $zn = sprintf("zone%06d.example", $z);
	foreach $ns (qw(2 3 4)) {
		print $rootdelegations "$zn.		NS	ns$ns.$zn.\n";
		print $rootdelegations "ns$ns.$zn.	A	10.53.0.$ns\n";		
	}
}
close $rootdelegations;
	
sub make_zones {
	my ($nsno, $slaved_from) = @_;
	my $namedconf = new FileHandle("ns$nsno/zones.conf", "w") or die;
	for ($z = 0; $z < $n_zones; $z++) {
		my $zn = sprintf("zone%06d.example", $z);
		if (defined($slaved_from)) {
			print $namedconf "zone \"$zn\" { type slave; " .
			    "file \"$zn.bk\"; masters { $slaved_from; }; };\n";
		} else {
			print $namedconf "zone \"$zn\" { " .
			    "type master; " .
			    "allow-update { any; }; " .
			    "file \"$zn.db\"; };\n";

			my $fn = "ns$nsno/$zn.db";
			my $f = new FileHandle($fn, "w") or die "open: $fn: $!";
			print $f "\$TTL 300
\@	IN SOA 	ns2.$zn. hostmaster 1 300 120 3600 86400
@	NS	ns2.$zn.
ns2.$zn.	A	10.53.0.2
@	NS	ns3.$zn.
ns3.$zn.	A	10.53.0.3
@	NS	ns4.$zn.
ns4.$zn.	A	10.53.0.4
	MX	10 mail1.isp.example.
	MX	20 mail2.isp.example.
";
			for ($i = 0; $i < $n_names; $i++) {
			    print $f sprintf("name%06d", $i) .
				"	A	10.0.0.1\n";
		    }
		    $f->close;
		}
	}
}
