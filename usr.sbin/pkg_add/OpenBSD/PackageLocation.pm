# ex:ts=8 sw=4:
# $OpenBSD: PackageLocation.pm,v 1.60 2022/05/08 13:31:40 espie Exp $
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
use OpenBSD::Paths;

sub new
{
	my ($class, $repository, $name) = @_;

	my $self = { repository => $repository, name => $repository->canonicalize($name) };
	bless $self, $class;
	return $self;

}

sub decorate
{
	my ($self, $plist) = @_;
	$self->{repository}->decorate($plist, $self);
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
	my $name = $self->name;
	if ($name =~ /^quirks\-/) {
		return $self->plist;
	}
	my $state = $self->{repository}{state};
	my $info = $self->{repository}->get_cached_info($name);
	if (defined $info && 
	    !defined $state->defines("CACHING_RECHECK")) {
		return $info;
	}
	my $result = $self->plist(\&OpenBSD::PackingList::UpdateInfoOnly);
	if (defined $info) {
		my $s1 = OpenBSD::Signature->from_plist($info);
		my $s2 = OpenBSD::Signature->from_plist($result);
		my $r = $s1->compare($s2, $state);
		if (defined $r && $r == 0) {
			$state->say("Cache comparison for #1 is okay", $name)
			    if $state->defines("TEST_CACHING_VERBOSE");
			return $result;
		} else {
			$state->fatal("Signatures differ cache=#1, regular=#2",
			    $s1->string, $s2->string);
		}
	}
	return $result;
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
		$self->{repository}->parse_problems($self->{errors}, undef, 
		    $self) if defined $self->{errors};
		undef $self->{errors};
		return;
	}
	require OpenBSD::Ustar;
	my $archive = OpenBSD::Ustar->new($fh, $self->{repository}{state});
	$archive->set_description($self->{repository}->url($self->{name}));
	$self->{_archive} = $archive;
	$self->_set_callback;

	if (defined $self->{_current_name}) {
		while (my $e = $self->{_archive}->next) {
			if ($e->{name} eq $self->{_current_name}) {
				$self->{_current} = $e;
				return $self;
			}
		}
	}
	return $self;
}

sub _set_callback
{
	my $self = shift;
	if (defined $self->{callback} && defined $self->{_archive}) {
		$self->{_archive}->set_callback($self->{callback});
	}
}

sub find_contents
{
	my $self = shift;

	while (my $e = $self->next) {
		if ($e->isFile && is_info_name($e->{name})) {
			if ($e->{name} eq CONTENTS ) {
				my $v = $e->contents;
				return $v;
			}
		} else {
			$self->unput;
			last;
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
		$self->{contents} = $self->find_contents;
	}

	return $self->{contents};
}

sub grab_info
{
	my $self = shift;
	my $dir = $self->{dir} = OpenBSD::Temp->dir;
	if (!defined $dir) {
		$self->{repository}{state}->fatal(OpenBSD::Temp->last_error);
	}

	my $c = $self->contents;
	if (!defined $c) {
		return 0;
	}

	if (! -f $dir.CONTENTS) {
		open my $fh, '>', $dir.CONTENTS or 
		    die "write to ",$dir.CONTENTS, ": ", $!;
		print $fh $self->contents;
		close $fh;
	}

	while (my $e = $self->next) {
		if ($e->isFile && is_info_name($e->{name})) {
			$e->{name} = $dir.$e->{name};
			undef $e->{mtime};
			undef $e->{atime};
			eval { $e->create; };
			if ($@) {
				unlink($e->{name});
				$@ =~ s/\s+at.*//o;
				$self->{repository}{state}->errprint('#1', $@);
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

sub forget
{
	my $self = shift;
	$self->wipe_info;
	$self->close_now;
}

sub wipe_info
{
	my $self = shift;
	$self->{repository}->wipe_info($self);
	$self->{repository}->close_now($self);
	delete $self->{contents};
	$self->deref;
	delete $self->{_current_name};
	delete $self->{update_info};
	delete $self->{_unput};
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
	delete $self->{fh};
	delete $self->{pid2};
	delete $self->{_archive};
	delete $self->{_current};
}

# proxy for archive operations
sub next
{
	my $self = shift;

	if (!$self->_opened) {
		return;
	}
	if (!$self->{_unput}) {
		$self->{_current} = $self->getNext;
		if (defined $self->{_current}) {
			$self->{_current_name} = $self->{_current}{name};
		} else {
			delete $self->{_current_name};
		}
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

	return $self->{_archive}->next;
}

sub skip
{
	my $self = shift;
	return $self->{_archive}->skip;
}

sub set_callback
{
	my ($self, $code) = @_;
	$self->{callback} = $code;
	$self->_set_callback;
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
