# ex:ts=8 sw=4:
# $OpenBSD: Handle.pm,v 1.14 2009/12/29 13:58:18 espie Exp $
#
# Copyright (c) 2007-2009 Marc Espie <espie@openbsd.org>
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

# fairly non-descriptive name. Used to store various package information
# during installs and updates.

use strict;
use warnings;

package OpenBSD::Handle;

use OpenBSD::PackageInfo;

use constant {
	BAD_PACKAGE => 1,
	CANT_INSTALL => 2,
	ALREADY_INSTALLED => 3,
	NOT_FOUND => 4
};

sub cleanup
{
	my ($self, $error) = @_;
	$self->{error} //= $error;
	if (defined $self->location) {
		if (defined $self->{error} &&
		    $self->{error} == ALREADY_INSTALLED) {
			$self->location->close_now;
		} elsif (defined $self->{error} &&
		    $self->{error} == CANT_INSTALL) {
			$self->location->close_with_client_error;
		}
		$self->location->wipe_info;
	}
	delete $self->{plist};
}

sub new
{
	my $class = shift;
	return bless {}, $class;
}

sub pkgname
{
	my $self = shift;
	if (!defined $self->{pkgname}) {
		if (defined $self->{plist}) {
			$self->{pkgname} = $self->{plist}->pkgname;
		} elsif (defined $self->{location}) {
			$self->{pkgname} = $self->{location}->name;
		} elsif (defined $self->{name}) {
			require OpenBSD::PackageName;

			$self->{pkgname} = 
			    OpenBSD::PackageName::url2pkgname($self->{name});
		}
	}

	return $self->{pkgname};
}

sub location
{
	return shift->{location};
}

sub plist
{
	return shift->{plist};
}

sub set_error
{
	my ($self, $error) = @_;
	$self->{error} = $error;
}

sub has_error
{
	my ($self, $error) = @_;
	if (!defined $self->{error}) {
		return undef;
	}
	if (defined $error) {
		return $self->{error} eq $error;
	}
	return $self->{error};
}

sub error_message
{
	my $self = shift;
	my $error = $self->{error};
	if ($error == BAD_PACKAGE) {
		return "bad package";
	} elsif ($error == CANT_INSTALL) {
		return "can't install";
	} elsif ($error == NOT_FOUND) {
		return "not found";
	} elsif ($error == ALREADY_INSTALLED) {
		return "already installed";
	} else {
		return "no error";
	}
}

sub complete_old
{
	my $self = shift;
	my $location = $self->{location};

	if (!defined $location) {
		$self->set_error(NOT_FOUND);
    	} else {
		my $plist = $location->plist;
		if (!defined $plist) {
			$self->set_error(BAD_PACKAGE);
		} else {
			$self->{plist} = $plist;
		}
	}
}

sub create_old
{

	my ($class, $pkgname, $state) = @_;
	my $self= $class->new;
	$self->{name} = $pkgname;

	require OpenBSD::PackageRepository::Installed;

	my $location = OpenBSD::PackageRepository::Installed->new->find($pkgname, $state->{arch});
	if (defined $location) {
		$self->{location} = $location;
	}
	$self->complete_old;

	return $self;
}

sub create_new
{
	my ($class, $pkg) = @_;
	my $handle = $class->new;
	$handle->{name} = $pkg;
	$handle->{tweaked} = 0;
	return $handle;
}

sub from_location
{
	my ($class, $location) = @_;
	my $handle = $class->new;
	$handle->{location} = $location;
	$handle->{tweaked} = 0;
	return $handle;
}

sub get_plist
{
	my ($handle, $state) = @_;

	my $location = $handle->{location};
	my $pkg = $handle->pkgname;

	if ($state->verbose >= 2) {
		$state->say($state->deptree_header($pkg), "parsing $pkg");
	}
	my $plist = $location->grabPlist;
	unless (defined $plist) {
		$state->say("Can't find CONTENTS from ", $location->url);
		$location->close_with_client_error;
		$location->wipe_info;
		$handle->set_error(BAD_PACKAGE);
		return;
	}
	if ($plist->localbase ne $state->{localbase}) {
		$state->say("Localbase mismatch: package has: ", $plist->localbase, " , user wants: ", $state->{localbase});
		$location->close_with_client_error;
		$location->wipe_info;
		$handle->set_error(BAD_PACKAGE);
		return;
	}
	my $pkgname = $handle->{pkgname} = $plist->pkgname;

	if ($pkg ne '-') {
		if (!defined $pkgname or $pkg ne $pkgname) {
			$state->say("Package name is not consistent ???");
			$location->close_with_client_error;
			$location->wipe_info;
			$handle->set_error(BAD_PACKAGE);
			return;
		}
	}
	$handle->{plist} = $plist;
}

sub get_location
{
	my ($handle, $state) = @_;

	my $name = $handle->{name};

	my $location = OpenBSD::PackageLocator->find($name, $state->{arch});
	if (!$location) {
		$state->print($state->deptree_header($name));
		$handle->set_error(NOT_FOUND);
		$handle->{tweaked} = 
		    OpenBSD::Add::tweak_package_status($handle->pkgname, 
			$state);
		if (!$handle->{tweaked}) {
			$state->say("Can't find $name");
		}
		return;
	}
	$handle->{location} = $location;
	$handle->{pkgname} = $location->name;
}

sub complete
{
	my ($handle, $state) = @_;

	return if $handle->has_error;

	if (!defined $handle->{location}) {
		$handle->get_location($state);
	}
	return if $handle->has_error;
	if (!defined $handle->{plist}) {
		$handle->get_plist($state);
	}
}

1;
