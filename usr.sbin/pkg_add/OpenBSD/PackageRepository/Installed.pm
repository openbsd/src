# ex:ts=8 sw=4:
# $OpenBSD: Installed.pm,v 1.17 2010/01/09 10:44:42 espie Exp $
#
# Copyright (c) 2007 Marc Espie <espie@openbsd.org>
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

sub parse_url
{
	my ($class, $path) = @_;
	$path .= '/' unless $path =~ m/\/$/;
	bless { path => $path }, $class;
}

sub parse_fullurl
{
	my ($class, $_) = @_;

	$class->strip_urlscheme(\$_) or return undef;
	return $class->parse_url($_);
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

package OpenBSD::PackageRepository::Installed;

our @ISA = (qw(OpenBSD::PackageRepositoryBase));

sub urlscheme
{
	return 'inst';
}

use OpenBSD::PackageInfo (qw(is_installed installed_info 
    installed_packages installed_stems installed_name));

my $singleton = bless {}, __PACKAGE__;
my $s2 = bless {all => 1}, __PACKAGE__;

sub new
{
	return $_[1] ? $s2 : $singleton;
}

sub relative_url
{
	my ($self, $name) = @_;
	return $name or '';
}

sub close
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
