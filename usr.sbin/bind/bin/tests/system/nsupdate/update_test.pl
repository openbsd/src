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

#
# Dynamic update test suite.
#
# Usage:
#
#   perl update_test.pl [-s server] [-p port] zone
#
# The server defaults to 127.0.0.1.
# The port defaults to 53.
#
# The "Special NS rules" tests will only work correctly if the
# zone has no NS records to begin with, or alternatively has a
# single NS record pointing at the name "ns1" (relative to
# the zone name).
#
# Installation notes:
#
# This program uses the Net::DNS::Resolver module.
# You can install it by saying
#
#    perl -MCPAN -e "install Net::DNS"
#
# $ISC: update_test.pl,v 1.7 2001/06/14 22:24:16 gson Exp $
#

use Getopt::Std;
use Net::DNS;
use Net::DNS::Update;
use Net::DNS::Resolver;

$opt_s = "127.0.0.1";
$opt_p = 53;

getopt('s:p:');

$res = new Net::DNS::Resolver;
$res->nameservers($opt_s);
$res->port($opt_p);
$res->defnames(0); # Do not append default domain.

@ARGV == 1 or die
    "usage: perl update_test.pl [-s server] [-p port] zone\n";

$zone = shift @ARGV;

my $failures = 0;

sub assert {
    my ($cond, $explanation) = @_;
    if (!$cond) {
	print "I:Test Failed: $explanation ***\n";
	$failures++
    }
}

sub test {
    my ($expected, @records) = @_;

    my $update = new Net::DNS::Update("$zone");

    foreach $rec (@records) {
	$update->push(@$rec);
    }

    $reply = $res->send($update);

    # Did it work?
    if (defined $reply) {
	my $rcode = $reply->header->rcode;
        assert($rcode eq $expected, "expected $expected, got $rcode");
    } else {
	print "I:Update failed: ", $res->errorstring, "\n";
    }
}

sub section {
    my ($msg) = @_;
    print "I:$msg\n";
}

section("Delete any leftovers from previous tests");
test("NOERROR", ["update", rr_del("a.$zone")]);
test("NOERROR", ["update", rr_del("b.$zone")]);
test("NOERROR", ["update", rr_del("c.$zone")]);
test("NOERROR", ["update", rr_del("d.$zone")]);
test("NOERROR", ["update", rr_del("e.$zone")]);
test("NOERROR", ["update", rr_del("f.$zone")]);
test("NOERROR", ["update", rr_del("ns.s.$zone")]);
test("NOERROR", ["update", rr_del("s.$zone")]);
test("NOERROR", ["update", rr_del("t.$zone")]);
test("NOERROR", ["update", rr_del("*.$zone")]);
test("NOERROR", ["update", rr_del("u.$zone")]);
test("NOERROR", ["update", rr_del("a.u.$zone")]);
test("NOERROR", ["update", rr_del("b.u.$zone")]);

section("Simple prerequisites in the absence of data");
# Name is in Use
test("NXDOMAIN", ["pre", yxdomain("a.$zone")]);
# RRset exists (value independent)
test("NXRRSET", ["pre", yxrrset("a.$zone A")]);
# Name is not in use
test("NOERROR", ["pre", nxdomain("a.$zone")]);
# RRset does not exist
test("NOERROR", ["pre", nxrrset("a.$zone A")]);
# RRset exists (value dependent)
test("NXRRSET", ["pre", yxrrset("a.$zone 300 A 73.80.65.49")]);


section ("Simple creation of data");
test("NOERROR", ["update", rr_add("a.$zone 300 A 73.80.65.49")]);

section ("Simple prerequisites in the presence of data");
# Name is in use
test("NOERROR", ["pre", yxdomain("a.$zone")]);
# RRset exists (value independent)
test("NOERROR", ["pre", yxrrset("a.$zone A")]);
# Name is not in use
test("YXDOMAIN", ["pre", nxdomain("a.$zone")]);
# RRset does not exist
test("YXRRSET", ["pre", nxrrset("a.$zone A")]);
# RRset exists (value dependent)
test("NOERROR", ["pre", yxrrset("a.$zone 300 A 73.80.65.49")]);

#
# Merging of RRsets
#
test("NOERROR", ["update", rr_add("a.$zone 300 A 73.80.65.50")]);

section("Detailed tests of \"RRset exists (value dependent)\" prerequisites");
test("NOERROR", ["pre",
		 yxrrset("a.$zone 300 A 73.80.65.49"),
		 yxrrset("a.$zone 300 A 73.80.65.50")]);
test("NOERROR", ["pre",
		 yxrrset("a.$zone 300 A 73.80.65.50"),
		 yxrrset("a.$zone 300 A 73.80.65.49")]);
test("NXRRSET", ["pre", yxrrset("a.$zone 300 A 73.80.65.49")]);
test("NXRRSET", ["pre", yxrrset("a.$zone 300 A 73.80.65.50")]);
test("NXRRSET", ["pre",
		 yxrrset("a.$zone 300 A 73.80.65.49"),
		 yxrrset("a.$zone 300 A 73.80.65.50"),
		 yxrrset("a.$zone 300 A 73.80.65.51")]);


section("Torture test of \"RRset exists (value dependent)\" prerequisites.");

test("NOERROR", ["update",
		 rr_add("e.$zone 300 A 73.80.65.49"),
		 rr_add("e.$zone 300 TXT 'one'"),
		 rr_add("e.$zone 300 A 73.80.65.50")]);
test("NOERROR", ["update",
		 rr_add("e.$zone 300 A 73.80.65.52"),
		 rr_add("f.$zone 300 A 73.80.65.52"),
		 rr_add("e.$zone 300 A 73.80.65.51")]);
test("NOERROR", ["update",
		 rr_add("e.$zone 300 TXT 'three'"),
		 rr_add("e.$zone 300 TXT 'two'")]);
test("NOERROR", ["update",
		 rr_add("e.$zone 300 MX 10 mail.$zone")]);

test("NOERROR", ["pre",
		 yxrrset("e.$zone 300 A 73.80.65.52"),
		 yxrrset("e.$zone 300 TXT 'two'"),
		 yxrrset("e.$zone 300 A 73.80.65.51"),
		 yxrrset("e.$zone 300 TXT 'three'"),
		 yxrrset("e.$zone 300 A 73.80.65.50"),
		 yxrrset("f.$zone 300 A 73.80.65.52"),
		 yxrrset("e.$zone 300 A 73.80.65.49"),
		 yxrrset("e.$zone 300 TXT 'one'")]);


section("Subtraction of RRsets");
test("NOERROR", ["update", rr_del("a.$zone 300 A 73.80.65.49")]);
test("NOERROR", ["pre",
		 yxrrset("a.$zone 300 A 73.80.65.50")]);

test("NOERROR", ["update", rr_del("a.$zone 300 A 73.80.65.50")]);
test("NOERROR", ["pre", nxrrset("a.$zone 300 A")]);
test("NOERROR", ["pre", nxdomain("a.$zone")]);

section("Other forms of deletion");
test("NOERROR", ["update", rr_add("a.$zone 300 A 73.80.65.49")]);
test("NOERROR", ["update", rr_add("a.$zone 300 A 73.80.65.50")]);
test("NOERROR", ["update", rr_add("a.$zone 300 MX 10 mail.$zone")]);
test("NOERROR", ["update", rr_del("a.$zone 300 A")]);
test("NOERROR", ["pre", nxrrset("a.$zone 300 A")]);
test("NOERROR", ["update", rr_add("a.$zone 300 A 73.80.65.49")]);
test("NOERROR", ["update", rr_add("a.$zone 300 A 73.80.65.50")]);
test("NOERROR", ["update", rr_del("a.$zone")]);
test("NOERROR", ["pre", nxdomain("a.$zone")]);

section("Case insensitivity");
test("NOERROR", ["update", rr_add("a.$zone 300 PTR foo.net.")]);
test("NOERROR", ["pre", yxrrset("A.$zone 300 PTR fOo.NeT.")]);

section("Special CNAME rules");
test("NOERROR", ["update", rr_add("b.$zone 300 CNAME foo.net.")]);
test("NOERROR", ["update", rr_add("b.$zone 300 A 73.80.65.49")]);
test("NOERROR", ["pre", yxrrset("b.$zone 300 CNAME foo.net.")]);
test("NOERROR", ["pre", nxrrset("b.$zone A")]);

test("NOERROR", ["update", rr_add("c.$zone 300 A 73.80.65.49")]);
test("NOERROR", ["update", rr_add("c.$zone 300 CNAME foo.net.")]);
test("NOERROR", ["pre", yxrrset("c.$zone A")]);
test("NOERROR", ["pre", nxrrset("c.$zone CNAME")]);

# XXX should test with SIG, KEY, NXT, too.

#
# Currently commented out because Net::DNS does not properly
# support WKS records.
#
#section("Special WKS rules");
#test("NOERROR", ["update", rr_add("c.$zone 300 WKS 73.80.65.49 TCP telnet ftp")]);
#test("NOERROR", ["update", rr_add("c.$zone 300 WKS 73.80.65.49 UDP telnet ftp")]);
#test("NOERROR", ["update", rr_add("c.$zone 300 WKS 73.80.65.50 TCP telnet ftp")]);
#test("NOERROR", ["update", rr_add("c.$zone 300 WKS 73.80.65.49 TCP smtp")]);
#test("NOERROR", ["pre",
#		 yxrrset("c.$zone 300 WKS 73.80.65.49 TCP smtp"),
#		 yxrrset("c.$zone 300 WKS 73.80.65.49 UDP telnet ftp"),
#		 yxrrset("c.$zone 300 WKS 73.80.65.50 TCP telnet ftp")]);


section("Special NS rules");

# Deleting the last NS record using "Delete an RR from an RRset"
# should fail at the zone apex and work elsewhere.  The pseudocode
# in RFC2136 says it should fail everywhere, but this is in conflict
# with the actual text.

# Apex
test("NOERROR", ["update",
		 rr_add("$zone 300 NS ns1.$zone"),
		 rr_add("$zone 300 NS ns2.$zone")]);
test("NOERROR", ["update", rr_del("$zone 300 NS ns1.$zone")]);
test("NOERROR", ["update", rr_del("$zone 300 NS ns2.$zone")]);
test("NOERROR", ["pre",
		 yxrrset("$zone 300 NS ns2.$zone")]);

# Non-apex
test("NOERROR", ["update", rr_add("n.$zone 300 NS ns1.$zone")]);
test("NOERROR", ["update", rr_del("n.$zone 300 NS ns1.$zone")]);
test("NOERROR", ["pre", nxrrset("n.$zone 300 NS")]);

# Other ways of deleting NS records should also fail at the apex
# and work elsewhere.

# Non-apex
test("NOERROR", ["update", rr_add("n.$zone 300 NS ns1.$zone")]);
test("NOERROR", ["update", rr_del("n.$zone 300 NS")]);
test("NOERROR", ["pre", nxrrset("n.$zone 300 NS")]);

test("NOERROR", ["update", rr_add("n.$zone 300 NS ns1.$zone")]);
test("NOERROR", ["pre", yxrrset("n.$zone 300 NS")]);
test("NOERROR", ["update", rr_del("n.$zone")]);
test("NOERROR", ["pre", nxrrset("n.$zone 300 NS")]);

# Apex
test("NOERROR", ["update", rr_del("$zone NS")]);
test("NOERROR", ["pre",
		 yxrrset("$zone 300 NS ns2.$zone")]);

test("NOERROR", ["update", rr_del("$zone")]);
test("NOERROR", ["pre",
		 yxrrset("$zone 300 NS ns2.$zone")]);

# They should not touch the SOA, either.

test("NOERROR", ["update", rr_del("$zone SOA")]);
test("NOERROR", ["pre", yxrrset("$zone SOA")]);


section("Idempotency");

test("NOERROR", ["update", rr_add("d.$zone 300 A 73.80.65.49")]);
test("NOERROR", ["pre", yxrrset("d.$zone 300 A 73.80.65.49")]);
test("NOERROR", ["update",
		 rr_add("d.$zone 300 A 73.80.65.49"),
		 rr_del("d.$zone A")]);
test("NOERROR", ["pre", nxrrset("d.$zone 300 A 73.80.65.49")]);

test("NOERROR", ["update", rr_del("d.$zone 300 A 73.80.65.49")]);
test("NOERROR", ["pre", nxrrset("d.$zone 300 A")]);
test("NOERROR", ["update",
		   rr_del("d.$zone 300 A"),
		   rr_add("d.$zone 300 A 73.80.65.49")]);

test("NOERROR", ["pre", yxrrset("d.$zone 300 A")]);

section("Out-of-zone prerequisites and updates");
test("NOTZONE", ["pre", yxrrset("a.somewhere.else. 300 A 73.80.65.49")]);
test("NOTZONE", ["update", rr_add("a.somewhere.else. 300 A 73.80.65.49")]);


section("Glue");
test("NOERROR", ["update", rr_add("s.$zone 300 NS ns.s.$zone")]);
test("NOERROR", ["update", rr_add("ns.s.$zone 300 A 73.80.65.49")]);
test("NOERROR", ["pre", yxrrset("ns.s.$zone 300 A 73.80.65.49")]);

section("Wildcards");
test("NOERROR", ["update", rr_add("*.$zone 300 MX 10 mail.$zone")]);
test("NOERROR", ["pre", yxrrset("*.$zone 300 MX 10 mail.$zone")]);
test("NXRRSET", ["pre", yxrrset("w.$zone 300 MX 10 mail.$zone")]);
test("NOERROR", ["pre", nxrrset("w.$zone MX")]);
test("NOERROR", ["pre", nxdomain("w.$zone")]);


section("SOA serial handling");

my $soatimers = "20 20 1814400 3600";

# Get the current SOA serial number.
my $query = $res->query($zone, "SOA");
my ($old_soa) = $query->answer;

my $old_serial = $old_soa->serial;

# Increment it by 10.
my $new_serial = $old_serial + 10;
if ($new_serial > 0xFFFFFFFF) {
    $new_serial -= 0x80000000;
    $new_serial -= 0x80000000;
}

# Replace the SOA with a new one.
test("NOERROR", ["update", rr_add("$zone 300 SOA mname1. . $new_serial $soatimers")]);

# Check that the SOA really got replaced.
($db_soa) = $res->query($zone, "SOA")->answer;
assert($db_soa->mname eq "mname1");

# Check that attempts to decrement the serial number are ignored.
$new_serial = $old_serial - 10;
if ($new_serial < 0) {
    $new_serial += 0x80000000;
    $new_serial += 0x80000000;
}
test("NOERROR", ["update", rr_add("$zone 300 SOA mname2. . $new_serial $soatimers")]);
assert($db_soa->mname eq "mname1");

# Check that attempts to leave the serial number unchanged are ignored.
($old_soa) = $res->query($zone, "SOA")->answer;
$old_serial = $old_soa->serial;
test("NOERROR", ["update", rr_add("$zone 300 SOA mname3. . $old_serial " .
				  $soatimers)]);
($db_soa) = $res->query($zone, "SOA")->answer;
assert($db_soa->mname eq "mname1");

#
# Currently commented out because Net::DNS does not properly
# support multiple strings in TXT records.
#
#section("Big data");
#test("NOERROR", ["update", rr_add("a.$zone 300 TXT aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc")]);
#test("NOERROR", ["update", rr_del("a.$zone 300 TXT aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc")]);
test("NOERROR", ["update", rr_add("a.$zone 300 TXT " . ("foo " x 3))]);

section("Updating TTLs only");

test("NOERROR", ["update", rr_add("t.$zone 300 A 73.80.65.49")]);
($a) = $res->query("t.$zone", "A")->answer;
$ttl = $a->ttl;
assert($ttl == 300, "incorrect TTL value $ttl != 300");
test("NOERROR", ["update",
		 rr_del("t.$zone 300 A 73.80.65.49"),
		 rr_add("t.$zone 301 A 73.80.65.49")]);
($a) = $res->query("t.$zone", "A")->answer;
$ttl = $a->ttl;
assert($ttl == 301, "incorrect TTL value $ttl != 301");

# Add an RR that is identical to an existing one except for the TTL.
# RFC2136 is not clear about what this should do; it says "duplicate RRs
# will be silently ignored" but is an RR differing only in TTL
# to be considered a duplicate or not?  The test assumes that it
# should not be considered a duplicate.
test("NOERROR", ["update", rr_add("t.$zone 302 A 73.80.65.50")]);
($a) = $res->query("t.$zone", "A")->answer;
$ttl = $a->ttl;
assert($ttl == 302, "incorrect TTL value $ttl != 302");

section("TTL normalization");

# The desired behaviour is that the old RRs get their TTL
# changed to match the new one.  RFC2136 does not explicitly
# specify this, but I think it makes more sense than the
# alternatives.

test("NOERROR", ["update", rr_add("t.$zone 303 A 73.80.65.51")]);
(@answers) = $res->query("t.$zone", "A")->answer;
$nanswers = scalar @answers;
assert($nanswers == 3, "wrong number of answers $nanswers != 3");
foreach $a (@answers) {
    $ttl = $a->ttl;
    assert($ttl == 303, "incorrect TTL value $ttl != 303");
}

section("Obscuring existing data by zone cut");
test("NOERROR", ["update", rr_add("a.u.$zone 300 A 73.80.65.49")]);
test("NOERROR", ["update", rr_add("b.u.$zone 300 A 73.80.65.49")]);
test("NOERROR", ["update", rr_add("u.$zone 300 TXT txt-not-in-nxt")]);
test("NOERROR", ["update", rr_add("u.$zone 300 NS ns.u.$zone")]);

test("NOERROR", ["update", rr_del("u.$zone 300 NS ns.u.$zone")]);

if ($failures) {
    print "I:$failures tests failed.\n";
} else {
    print "I:All tests successful.\n";
}
exit $failures;
