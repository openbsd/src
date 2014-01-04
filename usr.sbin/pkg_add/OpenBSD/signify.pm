# ex:ts=8 sw=4:
# $OpenBSD: signify.pm,v 1.4 2014/01/04 00:14:08 espie Exp $
#
# Copyright (c) 2013 Marc Espie <espie@openbsd.org>
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

package OpenBSD::signify;

use OpenBSD::PackageInfo;
use OpenBSD::Paths;
use File::Temp qw/mkstemp/;

my $header = "signify -- signature\n";
my $cmd = OpenBSD::Paths->signify;
my $suffix = ".sig";

sub compute_signature
{
	my ($plist, $state, $key) = @_;

	my $contents = $plist->infodir.CONTENTS;
	my $sigfile = $contents.$suffix;

	open my $fh, ">", $contents;
	$plist->write_no_sig($fh);
	close $fh;
	$state->system($cmd, '-s', $key, '-S', '--', $contents)
	    == 0 or die "probleme generating signature";
	open(my $sighandle, '<', $sigfile)
		or die "problem reading signature";
	my $header = <$sighandle>;
	my $sig = <$sighandle>;
	close($sighandle);
	unlink($sigfile);
	chomp $sig;
	return $sig;
}

sub check_signature
{
	my ($plist, $state) = @_;
	my $sig = $plist->get('digital-signature');
	my ($fh, $fname) = mkstemp("/tmp/pkgcontent.XXXXXXXXX");
	$plist->write_no_sig($fh);
	open(my $fh2, ">", $fname.$suffix);
	print $fh2 $header, $sig->{b64sig}, "\n";
	close $fh;
	close $fh2;
	my $pubkey;
	
	if ($state->defines('FW_UPDATE')) {
		$pubkey = OpenBSD::Paths->signifyfwkey;
	} else {
		$pubkey = OpenBSD::Paths->signifykey;
	}
	if ($plist->has('signer')) {
		my $signer = $plist->get('signer')->name;
		$pubkey = "/etc/signify/$signer.pub";
		if (!-f $pubkey) {
			$state->say("Unknown signer #1", $signer);
			return 0;
		}
	}
	if ($state->system(sub {
	    open STDOUT, ">", "/dev/null";},
	    $cmd, '-p', $pubkey, '-V', '--', $fname) != 0) {
	    	$state->log("Bad signature");
		return 0;
	}
	unlink $fname;
	unlink $fname.$suffix;
	return 1;
}

1;
