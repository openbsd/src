# ex:ts=8 sw=4:
# $OpenBSD: Installed.pm,v 1.5 2007/05/14 11:22:00 espie Exp $
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

sub match
{
	my ($self, $search, @filters) = @_;
	my @l = $search->match($self);
	while (my $filter = (shift @filters)) {
		last if @l == 0; # don't bother filtering empty list
		@l = $filter->filter(@l);
	}
	return @l;
}

package OpenBSD::PackageRepository::Installed;

our @ISA = (qw(OpenBSD::PackageRepositoryBase));

use OpenBSD::PackageInfo (qw(is_installed installed_info 
    installed_packages installed_stems));

my $singleton = bless {}, __PACKAGE__;
my $s2 = bless {all => 1}, __PACKAGE__;

sub new
{
	return $_[1] ? $s2 : $singleton;
}

sub find
{
	my ($repository, $name, $arch, $srcpath) = @_;
	my $self;

	if (is_installed($name)) {
		require OpenBSD::PackageLocation;

		$self = OpenBSD::PackageLocation->new($repository, $name);
		$self->{dir} = installed_info($name);
	}
	return $self;
}

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
