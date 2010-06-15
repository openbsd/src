# ex:ts=8 sw=4:
# $OpenBSD: x509.pm,v 1.6 2010/06/15 08:26:39 espie Exp $
#
# Copyright (c) 2003-2007 Marc Espie <espie@openbsd.org>
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

use strict;
use warnings;

package OpenBSD::x509;

use OpenBSD::PackageInfo;
use OpenBSD::Paths;
use MIME::Base64;
use File::Temp qw/mkstemp/;


sub compute_signature
{
	my ($plist, $cert, $key) = @_;

	open my $fh, ">", $plist->infodir.CONTENTS;
	$plist->write_no_sig($fh);
	close $fh;
	open(my $sighandle, "-|", OpenBSD::Paths->openssl, "smime", "-sign",
	    "-binary", "-signer", $cert ,"-in", $plist->infodir.CONTENTS,
	    "-inkey", $key, "-outform", "DEM") or die;
	my $sig;
	sysread($sighandle, $sig, 16384);
	close($sighandle) or die "problem generating signature $!";

	return encode_base64($sig, '');
}

sub dump_certificate_info
{
	my $fname2 = shift;

	open my $fh, "-|", OpenBSD::Paths->openssl, "asn1parse",
	    "-inform", "DEM", "-in", $fname2;
	my %want = map {($_, 1)}
	    qw(countryName localityName organizationName
	    organizationalUnitName commonName emailAddress);
	while (<$fh>) {
		if (m/\sprim\:\s+OBJECT\s*\:(.*)\s*$/) {
			my $objectname = $1;
			$_ = <$fh>;
			if (m/\sprim\:\s+[A-Z0-9]+\s*\:(.*)\s*$/) {
				if ($want{$objectname}) {
					print "$objectname=$1\n";
				}
			}
		}
	}
	close($fh);
}

sub print_certificate_info
{
	my $plist = shift;

	my ($fh, $fname) = mkstemp("/tmp/pkgsig.XXXXXXXXX");
	print $fh decode_base64($plist->{'digital-signature'}->{b64sig});
	close $fh;
	dump_certificate_info($fname);
	unlink $fname;
}

sub system_quiet
{
	my $r = fork;
	if (!defined $r) {
		return 1;
	} elsif ($r == 0) {
		open STDERR, ">/dev/null";
		exec {$_[0]} @_ or return 1;
	} else {
		waitpid($r, 0);
		return $?;
	}
}

sub check_signature
{
	my ($plist, $state) = @_;
	my $sig = $plist->get('digital-signature');
	if ($sig->{key} ne 'x509') {
		$state->log("Error: unknown signature style");
		return 0;
	}
	my ($fh, $fname) = mkstemp("/tmp/pkgcontent.XXXXXXXXX");
	my ($fh2, $fname2) = mkstemp("/tmp/pkgsig.XXXXXXXXX");
	$plist->write_no_sig($fh);
	print $fh2 decode_base64($sig->{b64sig});
	close $fh;
	close $fh2;
	if (system_quiet (OpenBSD::Paths->openssl, "smime", "-verify",
	    "-binary", "-inform", "DEM", "-in", $fname2, "-content", $fname,
	    "-CAfile", OpenBSD::Paths->pkgca, "-out", "/dev/null") != 0) {
	    	$state->log("Bad signature");
		return 0;
	}
	if ($state->verbose >= 2) {
		dump_certificate_info($fname2);
	}
	unlink $fname;
	unlink $fname2;
	return 1;
}

1;
