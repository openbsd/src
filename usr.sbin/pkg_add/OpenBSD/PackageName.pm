# ex:ts=8 sw=4:
# $OpenBSD: PackageName.pm,v 1.41 2010/01/19 14:58:53 espie Exp $
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

package OpenBSD::PackageName;

sub url2pkgname($)
{
	my $name = $_[0];
	$name =~ s|.*/||;
	$name =~ s|\.tgz$||;

	return $name;
}

# see packages-specs(7)
sub splitname
{
	my $_ = shift;
	if (/^(.*?)\-(\d.*)$/o) {
		my $stem = $1;
		my $rest = $2;
		my @all = split /\-/o, $rest;
		return ($stem, @all);
	} else {
		return ($_);
	}
}

my $cached = {};

sub from_string
{
	my ($class, $_) = @_;
	return $cached->{$_} //= $class->new_from_string($_);
}

sub new_from_string
{
	my ($class, $_) = @_;
	if (/^(.*?)\-(\d.*)$/o) {
		my $stem = $1;
		my $rest = $2;
		my @all = split /\-/o, $rest;
		my $version = OpenBSD::PackageName::version->from_string(shift @all);
		return bless {
			stem => $stem,
			version => $version,
			flavors => { map {($_, 1)} @all },
		}, "OpenBSD::PackageName::Name";
	} else {
		return bless {
			stem => $_,
		}, "OpenBSD::PackageName::Stem";
	}
}

sub splitstem
{
	my $_ = shift;
	if (/^(.*?)\-\d/o) {
		return $1;
	} else {
		return $_;
	}
}

sub is_stem
{
	my $_ = shift;
	if (m/\-\d/o || $_ eq '-') {
		return 0;
	} else {
		return 1;
	}
}

sub compile_stemlist
{
	my $hash = {};
	for my $n (@_) {
		my $stem = splitstem($n);
		$hash->{$stem} = {} unless defined $hash->{$stem};
		$hash->{$stem}->{$n} = 1;
	}
	bless $hash, "OpenBSD::PackageLocator::_compiled_stemlist";
}

sub avail2stems
{
	my @avail = @_;
	if (@avail == 0) {
		require OpenBSD::Error;

		OpenBSD::Error::Warn("No packages available in the PKG_PATH\n");
	}
	return OpenBSD::PackageName::compile_stemlist(@avail);
}

package OpenBSD::PackageLocator::_compiled_stemlist;

sub find
{
	my ($self, $stem) = @_;
	return keys %{$self->{$stem}};
}

sub add
{
	my ($self, $pkgname) = @_;
	my $stem = OpenBSD::PackageName::splitstem($pkgname);
	$self->{$stem}->{$pkgname} = 1;
}

sub delete
{
	my ($self, $pkgname) = @_;
	my $stem = OpenBSD::PackageName::splitstem($pkgname);
	delete $self->{$stem}->{$pkgname};
	if(keys %{$self->{$stem}} == 0) {
		delete $self->{$stem};
	}
}

sub find_partial
{
	my ($self, $partial) = @_;
	my @result = ();
	while (my ($stem, $pkgs) = each %$self) {
		next unless $stem =~ /\Q$partial\E/i;
		push(@result, keys %$pkgs);
	}
	return @result;
}
	
package OpenBSD::PackageName::dewey;

my $cache = {};

sub from_string
{
	my ($class, $string) = @_;
	my $o = bless { deweys => [ split(/\./o, $string) ]}, $class;
	for my $suffix (qw(rc beta pre pl)) {
		if ($o->{deweys}->[-1] =~ m/^(\d+)$suffix(\d*)$/) {
			$o->{deweys}->[-1] = $1;
			$o->{$suffix} = $2;
		}
	}
	return $o;
}

sub make
{
	my ($class, $string) = @_;
	return $cache->{$string} //= $class->from_string($string);
}

sub to_string
{
	my $self = shift;
	my $r = join('.', @{$self->{deweys}});
	for my $suffix (qw(pl pre beta rc)) {
		if (defined $self->{$suffix}) {
			$r .= $suffix . $self->{$suffix};
		}
	}
	return $r;
}

sub compare
{
	my ($a, $b) = @_;
	# Try a diff in dewey numbers first
	for (my $i = 0; ; $i++) {
		if (!defined $a->{deweys}->[$i]) {
			if (!defined $b->{deweys}->[$i]) {
				last;
			} else {
				return -1;
			}
		}
		if (!defined $b->{deweys}->[$i]) {
			return 1;
		}
		my $r = dewey_compare($a->{deweys}->[$i],
			$b->{deweys}->[$i]);
		return $r if $r != 0;
	}
	# finally try all the usual suspects
	# release candidates and beta and pre releases.
	for my $suffix (qw(rc beta pre pl)) {
		my $result = $suffix eq 'pl' ? 1 : -1;
		if (defined $a->{$suffix} && defined $b->{$suffix}) {
			return $a->{$suffix} <=> $b->{$suffix};
		}
		if (defined $a->{$suffix} && !defined $b->{$suffix}) {
			return $result;
		}
		if (!defined $a->{$suffix} && defined $b->{$suffix}) {
			return -$result;
		}
	}
	# give up: we don't know how to make a difference
	return 0;
}

sub dewey_compare
{
	my ($a, $b) = @_;
	# numerical comparison
	if ($a =~ m/^\d+$/o and $b =~ m/^\d+$/o) {
		return $a <=> $b;
	}
	# added lowercase letter
	if ("$a.$b" =~ m/^(\d+)([a-z]?)\.(\d+)([a-z]?)$/o) {
		my ($an, $al, $bn, $bl) = ($1, $2, $3, $4);
		if ($an != $bn) {
			return $an <=> $bn;
		} else {
			return $al cmp $bl;
		}
	}
	return $a cmp $b;
}

package OpenBSD::PackageName::version;

sub p
{
	my $self = shift;

	return defined $self->{p} ? $self->{p} : -1;
}

sub v
{
	my $self = shift;

	return defined $self->{v} ? $self->{v} : -1;
}

sub from_string
{
	my ($class, $string) = @_;
	my $o = bless {}, $class;
	if ($string =~ m/^(.*)v(\d+)$/o) {
		$o->{v} = $2;
		$string = $1;
	}
	if ($string =~ m/^(.*)p(\d+)$/o) {
		$o->{p} = $2;
		$string = $1;
	}
	$o->{dewey} = OpenBSD::PackageName::dewey->make($string);

	return $o;
}

sub to_string
{
	my $o = shift;
	my $string = $o->{dewey}->to_string;
	if (defined $o->{p}) {
		$string .= 'p'.$o->{p};
	}
	if (defined $o->{v}) {
		$string .= 'v'.$o->{v};
	}
	return $string;
}

sub pnum_compare
{
	my ($a, $b) = @_;
	return $a->p <=> $b->p;
}

sub compare
{
	my ($a, $b) = @_;
	# Simple case: epoch number
	if ($a->v != $b->v) {
		return $a->v <=> $b->v;
	}
	# Simple case: only p number differs
	if ($a->{dewey} eq $b->{dewey}) {
		return $a->pnum_compare($b);
	} 

	return $a->{dewey}->compare($b->{dewey});
}

package OpenBSD::PackageName::versionspec;
our @ISA = qw(OpenBSD::PackageName::version);

my $ops = {
	'<' => 'lt', 
	'<=' => 'le', 
	'>' => 'gt',
	'>=' => 'ge',
	'=' => 'eq' 
};

sub from_string
{
	my ($class, $s) = @_;
	my ($op, $version) = ('=', $s);
	if ($s =~ m/^(\>\=|\>|\<\=|\<|\=)(.*)$/) {
		($op, $version) = ($1, $2);
	}
	bless $class->SUPER::from_string($version), 
		"OpenBSD::PackageName::version::$ops->{$op}";
}

sub pnum_compare
{
	my ($spec, $b) = @_;
	if (!defined $spec->{p}) {
		return 0;
	} else {
		return $spec->SUPER::pnum_compare($b);
	}
}

sub is_exact
{
	return 0;
}
package OpenBSD::PackageName::version::lt;
our @ISA = qw(OpenBSD::PackageName::versionspec);
sub match
{
	my ($self, $b) = @_;
	-$self->compare($b) >= 0 ? 0 : 1;
}

package OpenBSD::PackageName::version::le;
our @ISA = qw(OpenBSD::PackageName::versionspec);
sub match
{
	my ($self, $b) = @_;
	-$self->compare($b) <= 0 ? 1 : 0;
}

package OpenBSD::PackageName::version::gt;
our @ISA = qw(OpenBSD::PackageName::versionspec);
sub match
{
	my ($self, $b) = @_;
	-$self->compare($b) > 0 ? 1 : 0;
}

package OpenBSD::PackageName::version::ge;
our @ISA = qw(OpenBSD::PackageName::versionspec);
sub match
{
	my ($self, $b) = @_;
	-$self->compare($b) >= 0 ? 1 : 0;
}

package OpenBSD::PackageName::version::eq;
our @ISA = qw(OpenBSD::PackageName::versionspec);
sub match
{
	my ($self, $b) = @_;
	-$self->compare($b) == 0 ? 1 : 0;
}

sub is_exact
{
	return 1;
}

package OpenBSD::PackageName::Stem;
sub to_string
{
	my $o = shift;
	return $o->{stem};
}

sub to_pattern
{
	my $o = shift;
	return $o->{stem}.'-*';
}

package OpenBSD::PackageName::Name;
sub flavor_string
{
	my $o = shift;
	return join('-', sort keys %{$o->{flavors}});
}

sub to_string
{
	my $o = shift;
	return join('-', $o->{stem}, $o->{version}->to_string, 
	    sort keys %{$o->{flavors}});
}

sub to_pattern
{
	my $o = shift;
	return join('-', $o->{stem}, '*', $o->flavor_string);
}

sub compare
{
	my ($a, $b) = @_;
	if ($a->{stem} ne $b->{stem} || $a->flavor_string ne $b->flavor_string) {
		return undef;
	}
	return $a->{version}->compare($b->{version});
}

1;
