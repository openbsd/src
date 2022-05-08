# ex:ts=8 sw=4:
# $OpenBSD: Installed.pm,v 1.44 2022/05/08 13:21:04 espie Exp $
#
# Copyright (c) 2007-2014 Marc Espie <espie@openbsd.org>
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

# XXX: we want to be able to load PackageRepository::Installed stand-alone,
# so we put the only common method into PackageRepositoryBase.
#
# later, when we load the base PackageRepository, we tweak the inheritance
# of PackageRepository::Installed to have full access...

package OpenBSD::PackageRepositoryBase;

my ($version, $current);

sub is_local_file
{
	return 0;
}

sub expand_locations
{
	my ($class, $string, $state) = @_;
	require OpenBSD::Paths;
	if ($string eq '%a') {
		return OpenBSD::Paths->machine_architecture;
	} elsif ($string eq '%v') {
		return OpenBSD::Paths->os_version;
	} elsif ($string eq '%m') {
		return join('/',
		    'pub/OpenBSD', 
		    '%c',
		    'packages', 
		    OpenBSD::Paths->machine_architecture);
	}
}

sub get_cached_info
{
	my ($repository, $name) = @_;
	if (defined $repository->{info_cache}) {
		return $repository->{info_cache}->get_cached_info($name);
	} else {
		return undef;
	}
}

sub setup_cache
{
	my ($repo, $setlist) = @_;

	my $state = $repo->{state};
	return if $state->defines("NO_CACHING");
	
	require OpenBSD::PackageRepository::Cache;

	$repo->{info_cache} = 
	    OpenBSD::PackageRepository::Cache->new($state, $setlist);
	# if we're on package-stable, assume this new quirks also works
	# with the corresponding release
	if (defined $repo->{release}) {
		my $url = $repo->urlscheme."://$repo->{host}$repo->{release}";
		my $r2 = $repo->parse_url(\$url, $state);
		$r2->{info_cache} = $repo->{info_cache};
	}
}

sub parse_url
{
	my ($class, $r, $state) = @_;

	my $path;

	if ($$r =~ m/^(.*?)\:(.*)/) {
		$path = $1;
		$$r = $2;
	} else {
		$path = $$r;
		$$r = '';
	}

	$path =~ s/\%[vam]\b/$class->expand_locations($&, $state)/ge;
	# make %c magical: if we're on a release, we expand into
	# stable, and leave the release dir for the full object with
	# host to push back
	my $release;
	if ($path =~ m/\%c\b/) {
		my $d = $state->defines('snap') ?
		    'snapshots' : OpenBSD::Paths->os_directory;
		if ($d ne 'snapshots' && $path =~ m,\%c/packages/,) {
			$release = $path;
			$release =~ s,\%c\b,$d,;
			$path =~ s,\%c/packages/,$d/packages-stable/,;
		} else {
			$path =~ s,\%c\b,$d,;
	    	}
	}
	$path .= '/' unless $path =~ m/\/$/;
	bless { path => $path, release => $release, state => $state }, $class;
}

sub parse_fullurl
{
	my ($class, $r, $state) = @_;

	$class->strip_urlscheme($r) or return undef;
	return $class->parse_url($r, $state);
}

sub strip_urlscheme
{
	my ($class, $r) = @_;
	if ($$r =~ m/^(.*?)\:(.*)$/) {
		my $scheme = lc($1);
		if ($scheme eq $class->urlscheme) {
			$$r = $2;
			return 1;
	    	}
	}
	return 0;
}

sub match_locations
{
	my ($self, $search, @filters) = @_;
	my $l = $search->match_locations($self);
	while (my $filter = (shift @filters)) {
		last if @$l == 0; # don't bother filtering empty list
		$l = $filter->filter_locations($l);
	}
	return $l;
}

sub url
{
	my ($self, $name) = @_;
	return $self->urlscheme.':'.$self->relative_url($name);
}

sub finish_and_close
{
	my ($self, $object) = @_;
	$self->close($object);
}

sub close_now
{
	my ($self, $object) = @_;
	$self->close($object, 0);
}

sub close_after_error
{
	my ($self, $object) = @_;
	$self->close($object, 1);
}

sub close_with_client_error
{
	my ($self, $object) = @_;
	$self->close($object, 1);
}

sub canonicalize
{
	my ($self, $name) = @_;

	if (defined $name) {
		$name =~ s/\.tgz$//o;
	}
	return $name;
}

sub new_location
{
	my ($self, @args) = @_;

	return $self->locationClassName->new($self, @args);
}

sub locationClassName
{ "OpenBSD::PackageLocation" }

sub locations_list
{
	my $self = shift;
	if (!defined $self->{locations}) {
		my $l = [];
		require OpenBSD::PackageLocation;

		for my $name (@{$self->list}) {
			push @$l, $self->new_location($name);
		}
		$self->{locations} = $l;
	}
	return $self->{locations};
}

sub reinitialize
{
}

sub decorate
{
	my ($self, $plist, $location) = @_;
	unless ($plist->has('url')) {
		OpenBSD::PackingElement::Url->add($plist, $location->url);
	}
	unless ($plist->has('signer')) {
		if (exists $location->{signer}) {
			OpenBSD::PackingElement::Signer->add($plist, 
			    $location->{signer});
		}
	}
	unless ($plist->has('digital-signature')) {
		if (exists $location->{signdate}) {
			OpenBSD::PackingElement::DigitalSignature->add($plist,
			    join(':', 'signify2', $location->{signdate}, 
			    	'external'));
		}
	}
}

package OpenBSD::PackageRepository::Installed;

our @ISA = (qw(OpenBSD::PackageRepositoryBase));

sub urlscheme
{
	return 'inst';
}

use OpenBSD::PackageInfo (qw(is_installed installed_info
    installed_packages installed_stems installed_name));

sub new
{
	my ($class, $all, $state) = @_;
	return bless { all => $all, state => $state }, $class;
}

sub relative_url
{
	my ($self, $name) = @_;
	$name or '';
}

sub close
{
}

sub make_error_file
{
}

sub canonicalize
{
	my ($self, $name) = @_;
	return installed_name($name);
}

sub find
{
	my ($repository, $name, $arch) = @_;
	my $self;

	if (is_installed($name)) {
		require OpenBSD::PackageLocation;

		$self = $repository->new_location($name);
		$self->{dir} = installed_info($name);
	}
	return $self;
}

sub locationClassName
{ "OpenBSD::PackageLocation::Installed" }

sub grabPlist
{
	my ($repository, $name, $arch, $code) = @_;
	require OpenBSD::PackingList;
	return  OpenBSD::PackingList->from_installation($name, $code);
}

sub available
{
	my $self = shift;
	return installed_packages($self->{all});
}

sub list
{
	my $self = shift;
	my @list = installed_packages($self->{all});
	return \@list;
}

sub stemlist
{
	return installed_stems();
}

sub wipe_info
{
}

sub may_exist
{
	my ($self, $name) = @_;
	return is_installed($name);
}

1;
