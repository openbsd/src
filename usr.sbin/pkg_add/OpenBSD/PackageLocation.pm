# ex:ts=8 sw=4:
# $OpenBSD: PackageLocation.pm,v 1.19 2010/05/10 09:17:55 espie Exp $
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

package OpenBSD::PackageLocation;

use OpenBSD::PackageInfo;
use OpenBSD::Temp;
use OpenBSD::Error;

sub new
{
	my ($class, $repository, $name, $arch) = @_;

	my $self = { repository => $repository, name => $repository->canonicalize($name)};
	if (defined $arch) {
		$self->{arch} = $arch;
	}
	bless $self, $class;
	return $self;

}

sub set_arch
{
	my ($self, $arch) = @_;

	$self->{arch} = $arch;
}

sub url
{
	my $self = shift;

	return $self->{repository}->url($self->name);
}

sub name
{
	my $self = shift;
	return $self->{name};
}

OpenBSD::Auto::cache(pkgname,
    sub {
	my $self = shift;
	return OpenBSD::PackageName->from_string($self->name);
    });

OpenBSD::Auto::cache(update_info,
    sub {
	my $self = shift;
	return $self->grabPlist(\&OpenBSD::PackingList::UpdateInfoOnly);
    });


# make sure self is opened and move to the right location if need be.
sub _opened
{
	my $self = shift;

	if (defined $self->{fh}) {
		return $self;
	}
	my $fh = $self->{repository}->open($self);
	if (!defined $fh) {
		$self->{repository}->parse_problems($self->{errors})
		    if defined $self->{errors};
		undef $self->{errors};
		return;
	}
	require OpenBSD::Ustar;
	my $archive = new OpenBSD::Ustar $fh;
	$self->{_archive} = $archive;

	if (defined $self->{_current}) {
		while (my $e = $self->{_archive}->next) {
			if ($e->{name} eq $self->{_current}->{name}) {
				$self->{_current} = $e;
				return $self;
			}
		}
	}
	return $self;
}

sub find_contents
{
	my $self = shift;

	while (my $e = $self->_next) {
		if ($e->isFile && is_info_name($e->{name})) {
			if ($e->{name} eq CONTENTS ) {
				return $e->contents;
			}
		} else {
			$self->unput;
			last;
		}
	}
}

sub find_fat_contents
{
	my $self = shift;

	while (my $e = $self->_next) {
		unless ($e->{name} =~ m/^(.*)\/\+CONTENTS$/o) {
			last;
		}
		my $prefix = $1;
		my $contents = $e->contents;
		require OpenBSD::PackingList;

		my $plist = OpenBSD::PackingList->fromfile(\$contents,
		    \&OpenBSD::PackingList::FatOnly);
		if (defined $self->name) {
			next if $plist->pkgname ne $self->name;
		}
		if ($plist->has('arch')) {
			if ($plist->{arch}->check($self->{arch})) {
				$self->{filter} = $prefix;
				return $contents;
			}
		}
	}
}

sub contents
{
	my $self = shift;
	if (!defined $self->{contents}) {
		if (!$self->_opened) {
			return;
		}
		$self->{contents} = $self->find_contents ||
		    $self->find_fat_contents;
	}

	return $self->{contents};
}

sub grab_info
{
	my $self = shift;
	my $dir = $self->{dir} = OpenBSD::Temp->dir;

	my $c = $self->contents;
	if (!defined $c) {
		return 0;
	}

	if (! -f $dir.CONTENTS) {
		open my $fh, '>', $dir.CONTENTS or die "Permission denied";
		print $fh $self->contents;
		close $fh;
	}

	while (my $e = $self->_next) {
		if ($e->isFile && is_info_name($e->{name})) {
			$e->{name} = $dir.$e->{name};
			eval { $e->create; };
			if ($@) {
				unlink($e->{name});
				$@ =~ s/\s+at.*//o;
				print STDERR $@;
				return 0;
			}
		} else {
			$self->unput;
			last;
		}
	}
	return 1;
}

sub grabPlist
{
	my ($self, $code) = @_;

	my $plist = $self->plist($code);
	if (defined $plist) {
		$self->wipe_info;
		$self->close_now;
		return $plist;
	} else {
		return;
	}
}

sub wipe_info
{
	my $self = shift;
	$self->{repository}->wipe_info($self);
}

sub info
{
	my $self = shift;

	if (!defined $self->{dir}) {
		$self->grab_info;
	}
	return $self->{dir};
}

sub plist
{
	my ($self, $code) = @_;
	require OpenBSD::PackingList;

	if (defined $self->{dir} && -f $self->{dir}.CONTENTS) {
		my $plist =
		    OpenBSD::PackingList->fromfile($self->{dir}.CONTENTS,
		    $code);
		$plist->set_infodir($self->{dir});
		return $plist;
	}
	if (my $value = $self->contents) {
		return OpenBSD::PackingList->fromfile(\$value, $code);
	}
	# hopeless
	$self->close_with_client_error;

	return;
}

sub close
{
	my ($self, $hint) = @_;
	$self->{repository}->close($self, $hint);
}

sub finish_and_close
{
	my $self = shift;
	$self->{repository}->finish_and_close($self);
}

sub close_now
{
	my $self = shift;
	$self->{repository}->close_now($self);
}

sub close_after_error
{
	my $self = shift;
	$self->{repository}->close_after_error($self);
}

sub close_with_client_error
{
	my $self = shift;
	$self->{repository}->close_with_client_error($self);
}

sub deref
{
	my $self = shift;
	$self->{fh} = undef;
	$self->{pid} = undef;
	$self->{pid2} = undef;
	$self->{_archive} = undef;
}

# proxy for archive operations
sub next
{
	my $self = shift;

	if (!defined $self->{dir}) {
		$self->grabInfoFiles;
	}
	return $self->_next;
}

sub _next
{
	my $self = shift;

	if (!$self->_opened) {
		return;
	}
	if (!$self->{_unput}) {
		$self->{_current} = $self->getNext;
	} else {
		$self->{_unput} = 0;
	}
	return $self->{_current};
}

sub unput
{
	my $self = shift;
	$self->{_unput} = 1;
}

sub getNext
{
	my $self = shift;

	my $e = $self->{_archive}->next;
	if (defined $self->{filter}) {
		if ($e->{name} =~ m/^(.*?)\/(.*)$/o) {
			my ($beg, $name) = ($1, $2);
			if (index($beg, $self->{filter}) == -1) {
				return $self->getNext;
			}
			$e->{name} = $name;
			if ($e->isHardLink) {
				$e->{linkname} =~ s/^(.*?)\///o;
			}
		}
	}
	return $e;
}

sub skip
{
	my $self = shift;
	return $self->{_archive}->skip;
}

package OpenBSD::PackageLocation::Installed;
our @ISA = qw(OpenBSD::PackageLocation);


sub info
{
	my $self = shift;
	require OpenBSD::PackageInfo;
	$self->{dir} = OpenBSD::PackageInfo::installed_info($self->name);
}

sub plist
{
	my ($self, $code) = @_;
	require OpenBSD::PackingList;
	return OpenBSD::PackingList->from_installation($self->name, $code);
}

1;
