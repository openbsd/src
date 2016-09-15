#! /usr/bin/perl
# ex:ts=8 sw=4:
# $OpenBSD: Signer.pm,v 1.8 2016/09/15 13:14:03 espie Exp $
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
use OpenBSD::PackageInfo;

my $h = {
	x509 => 'OpenBSD::Signer::X509',
	signify => 'OpenBSD::Signer::SIGNIFY',
	signify2 => 'OpenBSD::Signer::SIGNIFY2',
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

sub sign
{
	my ($signer, $pkg, $state, $tmp) = @_;

	my $dir = $pkg->info;
	my $plist = OpenBSD::PackingList->fromfile($dir.CONTENTS);
	# In incremental mode, don't bother signing known packages
	$plist->set_infodir($dir);
	$state->add_signature($plist);
	$plist->save;
	my $wrarc = $state->create_archive($tmp, ".");

	my $fh;
	my $url = $pkg->url;
	my $buffer;

	if (defined $pkg->{length} and 
	    $url =~ s/^file:// and open($fh, "<", $url) and
	    $fh->seek($pkg->{length}, 0) and $fh->read($buffer, 2)
	    and $buffer eq "\x1f\x8b" and $fh->seek($pkg->{length}, 0)) {
	    	#$state->say("FAST #1", $plist->pkgname);
		$wrarc->destdir($pkg->info);
		my $e = $wrarc->prepare('+CONTENTS');
		$e->write;
		close($wrarc->{fh});
		delete $wrarc->{fh};

		open(my $fh2, ">>", $tmp) or 
		    $state->fatal("Can't append to #1", $tmp);
		require File::Copy;
		File::Copy::copy($fh, $fh2) or 
		    $state->fatal("Error in copy #1", $!);
		close($fh2);
	} else {
	    	#$state->say("SLOW #1", $plist->pkgname);
		$plist->copy_over($state, $wrarc, $pkg);
		$wrarc->close;
	}
	close($fh) if defined $fh;

	$pkg->wipe_info;
}

package OpenBSD::Signer::X509;
our @ISA = qw(OpenBSD::Signer);
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
our @ISA = qw(OpenBSD::Signer);
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

package OpenBSD::Signer::SIGNIFY2;
our @ISA = qw(OpenBSD::Signer);
sub new
{
	my ($class, $state, @p) = @_;
	if (@p != 2 || !-f $p[1]) {
		$state->usage("$p[0] signature wants -s privkey");
	}
	my $o = bless {privkey => $p[1]}, $class;
	return $o;
}

sub sign
{
	my ($signer, $pkg, $state, $tmp) = @_;
	my $privkey = $signer->{privkey};
 	my $url = $pkg->url;
	$url =~ s/^file://;
	$state->system(OpenBSD::Paths->signify, '-zS', '-s', $privkey, '-m', $url, '-x', $tmp);
}

# specific parameter handling plus element creation
package OpenBSD::CreateSign::State;
our @ISA = qw(OpenBSD::AddCreateDelete::State);

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
