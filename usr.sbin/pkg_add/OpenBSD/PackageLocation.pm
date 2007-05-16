# ex:ts=8 sw=4:
# $OpenBSD: PackageLocation.pm,v 1.10 2007/05/16 07:18:55 espie Exp $
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

sub new
{
	my ($class, $repository, $name, $arch) = @_;

	if (defined $name) {
		$name =~ s/\.tgz$//;
	}
	my $self = { repository => $repository, name => $name, arch => $arch};
	bless $self, $class;
#	print STDERR "Built location ", $self->stringize, "\n";
	return $self;

}

sub set_arch
{
	my ($self, $arch) = @_;

	$self->{arch} = $arch;
}

sub stringize
{
	my $self = shift;

	return $self->{repository}->stringize($self->{name});
}

sub openArchive
{
	my $self = shift;

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
}

sub grabInfoFiles
{
	my $self = shift;
	my $dir = $self->{dir} = OpenBSD::Temp::dir();

	if (defined $self->{contents} && ! -f $dir.CONTENTS) {
		open my $fh, '>', $dir.CONTENTS or die "Permission denied";
		print $fh $self->{contents};
		close $fh;
	}

	while (my $e = $self->intNext) {
		if ($e->isFile && is_info_name($e->{name})) {
			$e->{name}=$dir.$e->{name};
			eval { $e->create(); };
			if ($@) {
				unlink($e->{name});
				$@ =~ s/\s+at.*//;
				print STDERR $@;
				return 0;
			}
		} else {
			$self->unput();
			last;
		}
	}
	return 1;
}

sub scanPackage
{
	my $self = shift;
	while (my $e = $self->intNext) {
		if ($e->isFile && is_info_name($e->{name})) {
			if ($e->{name} eq CONTENTS && !defined $self->{dir}) {
				$self->{contents} = $e->contents();
				last;
			}
			if (!defined $self->{dir}) {
				$self->{dir} = OpenBSD::Temp::dir();
			}
			$e->{name}=$self->{dir}.$e->{name};
			eval { $e->create; };
			if ($@) {
				unlink($e->{name});
				$@ =~ s/\s+at.*//;
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

	my $pkg = $self->openPackage;
	if (defined $pkg) {
		my $plist = $self->plist($code);
		$pkg->wipe_info;
		$pkg->close_now;
		return $plist;
	} else {
		return;
	}
}

sub openPackage
{
	my $self = shift;
	my $arch = $self->{arch};
	if (!$self->openArchive) {
		return;
	}
	$self->scanPackage;

	if (defined $self->{contents}) {
		return $self;
	} 

	# maybe it's a fat package.
	while (my $e = $self->intNext) {
		unless ($e->{name} =~ m/\/\+CONTENTS$/) {
			last;
		}
		my $prefix = $`;
		my $contents = $e->contents;
		require OpenBSD::PackingList;

		my $plist = OpenBSD::PackingList->fromfile(\$contents, 
		    \&OpenBSD::PackingList::FatOnly);
		if (defined $self->{name}) {
			next if $plist->pkgname ne $self->{name};
		}
		if ($plist->has('arch')) {
			if ($plist->{arch}->check($arch)) {
				$self->{filter} = $prefix;
				bless $self, "OpenBSD::FatPackageLocation";
				$self->{contents} = $contents;
				return $self;
			}
		}
	}
	# hopeless
	$self->close_with_client_error;
	$self->wipe_info;
	return;
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
		$self->grabInfoFiles;
	}
	return $self->{dir};
}

sub plist
{
	my ($self, $code) = @_;

	require OpenBSD::PackingList;

	if (defined $self->{contents}) {
		my $value = $self->{contents};
		return OpenBSD::PackingList->fromfile(\$value, $code);
	} elsif (defined $self->{dir} && -f $self->{dir}.CONTENTS) {
		return OpenBSD::PackingList->fromfile($self->{dir}.CONTENTS, 
		    $code);
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

sub reopen
{
	my $self = shift;
	if (!$self->openArchive) {
		return;
	}
	while (my $e = $self->{_archive}->next) {
		if ($e->{name} eq $self->{_current}->{name}) {
			$self->{_current} = $e;
			return $self;
		}
	}
	return;
}

# proxy for archive operations
sub next
{
	my $self = shift;

	if (!defined $self->{dir}) {
		$self->grabInfoFiles;
	}
	return $self->intNext;
}

sub intNext
{
	my $self = shift;

	if (!defined $self->{fh}) {
		if (!$self->reopen) {
			return;
		}
	}
	if (!$self->{_unput}) {
		$self->{_current} = $self->getNext;
	}
	$self->{_unput} = 0;
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

	return $self->{_archive}->next;
}

sub skip
{
	my $self = shift;
	return $self->{_archive}->skip;
}

package OpenBSD::FatPackageLocation;
our @ISA=qw(OpenBSD::PackageLocation);

sub getNext
{
	my $self = shift;

	my $e = $self->SUPER::getNext;
	if ($e->{name} =~ m/^(.*?)\/(.*)$/) {
		my ($beg, $name) = ($1, $2);
		if (index($beg, $self->{filter}) == -1) {
			return $self->next;
		}
		$e->{name} = $name;
		if ($e->isHardLink) {
			$e->{linkname} =~ s/^(.*?)\///;
		}
	}
	return $e;
}

1;
