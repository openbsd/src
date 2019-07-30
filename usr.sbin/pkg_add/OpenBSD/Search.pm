# ex:ts=8 sw=4:
# $OpenBSD: Search.pm,v 1.29 2016/06/14 15:41:31 espie Exp $
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

package OpenBSD::Search;
sub match_locations
{
	my ($self, $o) = @_;
	require OpenBSD::PackageLocation;

	my @l = map {$o->new_location($_)} $self->match($o);
	return \@l;
}

package OpenBSD::Search::PkgSpec;
our @ISA=(qw(OpenBSD::Search));

sub filter
{
	my ($self, @list) = @_;
	return $self->{spec}->match_ref(\@list);
}

sub filter_libs
{
	my ($self, @list) = @_;
	return $self->{spec}->match_libs_ref(\@list);
}

sub match_locations
{
	my ($self, $o) = @_;
	return $self->{spec}->match_locations($o->locations_list);
}

sub filter_locations
{
	my ($self, $l) = @_;
	return $self->{spec}->match_locations($l);
}

sub new
{
	my ($class, $pattern, $with_partial) = @_;
	require OpenBSD::PkgSpec;

	bless { spec => $class->spec_class->new($pattern, $with_partial)}, 
	    $class;
}

sub add_pkgpath_hint
{
	my ($self, $pkgpath) = @_;
	$self->{pkgpath} = $pkgpath;
	return $self;
}

sub spec_class
{ "OpenBSD::PkgSpec" }

sub is_valid
{
	my $self = shift;
	return $self->{spec}->is_valid;
}

package OpenBSD::Search::Exact;
our @ISA=(qw(OpenBSD::Search::PkgSpec));
sub spec_class
{ "OpenBSD::PkgSpec::Exact" }

package OpenBSD::Search::Stem;
our @ISA=(qw(OpenBSD::Search));

sub new
{
	my ($class, $stem) = @_;
	if ($stem =~ m/^(.*)\%(.*)/) {
		return ($class->_new($1), 
		    OpenBSD::Search::FilterLocation->match_partialpath($2));
	} else {
		return $class->_new($stem);
	}
}

sub _new
{
	my ($class, $stem) = @_;

	if ($stem =~ m/^(.*)\-\-(.*)/) {
		# XXX
		return OpenBSD::Search::Exact->new("$1-*-$2");
    	}
	return bless {"$stem" => 1}, $class;
}

sub split
{
	my ($class, $pkgname) = @_;
	require OpenBSD::PackageName;

	return $class->new(OpenBSD::PackageName::splitstem($pkgname));
}

sub add_stem
{
	my ($self, $extra) = @_;
	$self->{$extra} = 1;

}

sub match
{
	my ($self, $o) = @_;

	my @r = ();
	for my $k (keys %$self) {
		push(@r, $o->stemlist->find($k));
	}
	return @r;
}

sub _keep
{
	my ($self, $stem) = @_;
	return defined $self->{$stem};
}

sub filter
{
	my ($self, @l) = @_;
	my @result = ();
	require OpenBSD::PackageName;
	for my $pkg (@l) {
		if ($self->_keep(OpenBSD::PackageName::splitstem($pkg))) {
			push(@result, $pkg);
		}
	}
	return @result;
}

package OpenBSD::Search::PartialStem;
our @ISA=(qw(OpenBSD::Search::Stem));

sub match
{
	my ($self, $o) = @_;
	my @r = ();
	for my $k (keys %$self) {
		push(@r, $o->stemlist->find_partial($k));
	}
	return @r;
}

sub _keep
{
	my ($self, $stem) = @_;
	for my $partial (keys %$self) {
		if ($stem =~ /\Q$partial\E/) {
			return 1;
		}
	}
	return 0;
}

package OpenBSD::Search::FilterLocation;
our @ISA=(qw(OpenBSD::Search));
sub new
{
	my ($class, $code) = @_;

	return bless {code => $code}, $class;
}

sub filter_locations
{
	my ($self, $l) = @_;
	return &{$self->{code}}($l);
}

sub more_recent_than
{
	my ($class, $name, $rfound) = @_;
	require OpenBSD::PackageName;

	my $f = OpenBSD::PackageName->from_string($name);

	return $class->new(
sub {
	my $l = shift;
	my $r = [];
	for my $e (@$l) {
		if ($f->{version}->compare($e->pkgname->{version}) <= 0) {
			push(@$r, $e);
		}
		if (ref $rfound) {
			$$rfound = 1;
		}
	}
	return $r;
	});
}

sub keep_most_recent
{
	my $class = shift;
	return $class->new(
sub {
	my $l = shift;
	# no need to filter
	return $l if @$l <= 1;

	require OpenBSD::PackageName;
	my $h = {};
	# we have to prove we have to keep it
	while (my $e = pop @$l) {
		my $stem = $e->pkgname->{stem};
		my $keep = 1;
		# so let's compare with every element in $h with the same stem
		for my $f (@{$h->{$e->pkgname->{stem}}}) {
			# if this is not the same flavors,
			# we don't filter
			if ($f->pkgname->flavor_string ne $e->pkgname->flavor_string) {
				next;
			}
			# okay, now we need to prove there's a common pkgpath
			if (!$e->update_info->match_pkgpath($f->update_info)) {
				next;
			}

			if ($f->pkgname->{version}->compare($e->pkgname->{version}) < 0) {
			    $f = $e;
			}
			$keep = 0;
			last;

		}
		if ($keep) {
			push(@{$h->{$e->pkgname->{stem}}}, $e);
		}
	}
	my $largest = [];
	push @$largest, map {@$_} values %$h;
	return $largest;
}
	);
}

sub match_partialpath
{
	my ($class, $subdir) = @_;
	return $class->new(
sub {
	my $l = shift;
	if (@$l == 0) {
		return $l;
	}
	my @l2 = ();
	for my $loc (@$l) {
		if (!$loc) {
			next;
		}
		my $p2 = $loc->update_info;
		if (!$p2) {
			next;
		}
		if ($p2->pkgpath->partial_match($subdir)) {
			push(@l2, $loc);
		}
	}
	return \@l2;
}
	);
}

1;
