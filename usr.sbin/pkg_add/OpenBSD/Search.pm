# ex:ts=8 sw=4:
# $OpenBSD: Search.pm,v 1.5 2007/05/19 09:45:33 espie Exp $
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

package OpenBSD::Search;
sub match_locations
{
	my ($self, $o) = @_;
	require OpenBSD::PackageLocation;

	return map {OpenBSD::PackageLocation->new($o, $_)} $self->match($o);
}

# XXX this is not efficient
sub filter_locations
{
	my $self = shift;
	my @r = ();
	while (my $loc = shift @_) {
		if ($self->filter($loc->{name})) {
			push(@r, $loc);
		}
	}
	return @r;
}

package OpenBSD::Search::PkgSpec;
our @ISA=(qw(OpenBSD::Search));

sub match_ref
{
	my ($self, $r) = @_;
	my @l = ();

	for my $subpattern (@{$self->{patterns}}) {
		require OpenBSD::PkgSpec;
		push(@l, OpenBSD::PkgSpec::subpattern_match($subpattern, $r));
	}
	return @l;
}

sub match
{
	my ($self, $o) = @_;
	return $self->match_ref($o->list);
}

sub filter
{
	my ($self, @list) = @_;
	return $self->match_ref(\@list);
}

sub new
{
	my ($class, $pattern) = @_;
	my @l = split /\|/, $pattern;
	bless { patterns => \@l }, $class;
}

sub add_pkgpath_hint
{
	my ($self, $pkgpath) = @_;
	$self->{pkgpath} = $pkgpath;
}

package OpenBSD::Search::Stem;
our @ISA=(qw(OpenBSD::Search));

sub new
{
	my ($class, $stem) = @_;

	return bless {stem => $stem}, $class;
}

sub split
{
	my ($class, $pkgname) = @_;
	require OpenBSD::PackageName;

	return $class->new(OpenBSD::PackageName::splitstem($pkgname));
}

sub match
{
	my ($self, $o) = @_;
	return $o->stemlist->find($self->{stem});
}

sub _keep
{
	my ($self, $stem) = @_;
	return $self->{stem} eq $stem;
}

sub filter
{
	my ($self, @l) = @_;
	my @result = ();
	require OpenBSD::PackageName;
	for my $pkg (@l) {
		if ($self->_keep(OpenBSD::PackageName::splitstem($pkgname))) {
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
	return $o->stemlist->find_partial($self->{stem});
}

sub _keep
{
	my ($self, $stem) = @_;
	my $partial = $self->{stem};
	return $stem =~ /\Q$partial\E/;
}

package OpenBSD::Search::Filter;
our @ISA=(qw(OpenBSD::Search));

sub new
{
	my ($class, $code) = @_;

	return bless {code => $code}, $class;
}

sub filter
{
	my ($self, @l) = @_;
	return &{$self->{code}}(@l);
}

sub keep_most_recent
{
	my $class = shift;
	require OpenBSD::PackageName;
	
	return $class->new(\&OpenBSD::PackageName::keep_most_recent);
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
	my ($self, @l) = @_;
	return &{$self->{code}}(@l);
}

1;
