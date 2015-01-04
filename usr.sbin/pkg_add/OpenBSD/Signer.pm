#! /usr/bin/perl
# ex:ts=8 sw=4:
# $OpenBSD: Signer.pm,v 1.6 2015/01/04 14:10:20 espie Exp $
#
# Copyright (c) 2003-2014 Marc Espie <espie@openbsd.org>
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

# code necessary to create signed package

# the factory that chooses what method to use to sign things
package OpenBSD::Signer;

my $h = {
	x509 => 'OpenBSD::Signer::X509',
	signify => 'OpenBSD::Signer::SIGNIFY',
};

sub factory
{
	my ($class, $state) = @_;

	my @p = @{$state->{signature_params}};

	if (defined $h->{$p[0]}) {
		return $h->{$p[0]}->new($state, @p);
	} else {
		$state->usage("Unknown signature scheme $p[0]");
	}
}

package OpenBSD::Signer::X509;
sub new
{
	my ($class, $state, @p) = @_;

	if (@p != 3 || !-f $p[1] || !-f $p[2]) {
		$state->usage("$p[0] signature wants -s cert -s privkey");
	}
	bless {cert => $p[1], privkey => $p[2]}, $class;
}

sub new_sig
{
	require OpenBSD::x509;
	return OpenBSD::PackingElement::DigitalSignature->blank('x509');
}

sub compute_signature
{
	my ($self, $state, $plist) = @_;
	return OpenBSD::x509::compute_signature($plist, $self->{cert}, 
	    $self->{privkey});
}

package OpenBSD::Signer::SIGNIFY;
sub new
{
	my ($class, $state, @p) = @_;
	if (@p != 2 || !-f $p[1]) {
		$state->usage("$p[0] signature wants -s privkey");
	}
	my $o = bless {privkey => $p[1]}, $class;
	my $signer = $o->{privkey};
	$signer =~ s/\.sec$//;
	my $pubkey = "$signer.pub";
	$signer =~ s,.*/,,;
	$o->{signer} = $signer;
	if (!-f $pubkey) {
		$pubkey =~ s,.*/,/etc/signify/,;
		if (!-f $pubkey) {
			$state->errsay("warning: public key not found");
			return $o;
		}
	}
	$o->{pubkey} = $pubkey;
	return $o;
}

sub new_sig
{
	require OpenBSD::signify;
	return OpenBSD::PackingElement::DigitalSignature->blank('signify');
}

sub compute_signature
{
	my ($self, $state, $plist) = @_;

	OpenBSD::PackingElement::Signer->add($plist, $self->{signer});

	return OpenBSD::signify::compute_signature($plist, $state, 
	    $self->{privkey}, $self->{pubkey});
}

# specific parameter handling plus element creation
package OpenBSD::CreateSign::State;
our @ISA = qw(OpenBSD::AddCreateDelete::State);

sub handle_options
{
	my ($state, $opt_string, @usage) = @_;
	$state->{opt}{s} = 
	    sub { 
	    	push(@{$state->{signature_params}}, shift);
	    };
	$state->{no_exports} = 1;
	$state->SUPER::handle_options($opt_string.'s:', @usage);
	if (defined $state->{signature_params}) {
		$state->{signer} = OpenBSD::Signer->factory($state);
	}
}

sub create_archive
{
	my ($state, $filename, $dir) = @_;
	require IO::Compress::Gzip;
	my $level = $state->{subst}->value('COMPRESSION_LEVEL') // 6;
	my $fh = IO::Compress::Gzip->new($filename, 
	    -Level => $level, -Time => 0) or
		$state->fatal("Can't create archive #1: #2", $filename, $!);
	$state->{archive_filename} = $filename;
	return OpenBSD::Ustar->new($fh, $state, $dir);
}

sub new_gstream
{
	my $state = shift;
	close($state->{archive}{fh});
	my $level = $state->{subst}->value('COMPRESSION_LEVEL') // 6;
	$state->{archive}{fh} =IO::Compress::Gzip->new(
	    $state->{archive_filename}, 
	    -Level => $level, -Time => 0, -Append => 1) or
		$state->fatal("Can't append to archive #1: #2", 
		    $state->{archive_filename}, $!);
}

sub add_signature
{
	my ($state, $plist) = @_;

	if ($plist->has('digital-signature') || $plist->has('signer')) {
		if ($state->defines('resign')) {
			if ($state->defines('nosig')) {
				$state->errsay("NOT CHECKING DIGITAL SIGNATURE FOR #1",
				    $plist->pkgname);
			} else {
				if (!$plist->check_signature($state)) {
					$state->fatal("#1 is corrupted",
					    $plist->pkgname);
				}
			}
			$state->errsay("Resigning #1", $plist->pkgname);
			delete $plist->{'digital-signature'};
			delete $plist->{signer};
		}
	}

	my $sig = $state->{signer}->new_sig;
	$sig->add_object($plist);
	$sig->{b64sig} = $state->{signer}->compute_signature($state, $plist);
}

sub ntodo
{
	my ($self, $offset) = @_;
	return sprintf("%u/%u", $self->{done}-$offset, $self->{total});
}

1;
