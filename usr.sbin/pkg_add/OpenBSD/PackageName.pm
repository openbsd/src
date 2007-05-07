# ex:ts=8 sw=4:
# $OpenBSD: PackageName.pm,v 1.16 2007/05/07 13:51:58 espie Exp $
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
	local $_ = shift;
	if (/\-(?=\d)/) {
		my $stem = $`;
		my $rest = $';
		my @all = split /\-/, $rest;
		return ($stem, @all);
	} else {
		return ($_);
	}
}

sub from_string
{
	my $class = shift;
	local $_ = shift;
	if (/\-(?=\d)/) {
		my $stem = $`;
		my $rest = $';
		my @all = split /\-/, $rest;
		my $version = OpenBSD::PackageName::version->from_string(shift @all);
		return bless {
			stem => $stem,
			version => $version,
			flavors => \@all
		}, "OpenBSD::PackageName::Name";
	} else {
		return bless {
			stem => $_,
		}, "OpenBSD::PackageName::Stem";
	}
}

sub splitstem
{
	local $_ = shift;
	if (/\-(?=\d)/) {
		return $`;
	} else {
		return $_;
	}
}

sub is_stem
{
	local $_ = shift;
	if (m/\-\d/ || $_ eq '-') {
		return 0;
	} else {
		return 1;
	}
}

sub splitp
{
	local $_ = shift;

	if (/^(.*\-\d[^-]*)p(\d+)/) {
		return ($1.$', $2);
	} else {
		return ($_,-1);
	}
}

sub rebuildp
{
	my ($pkg, $p) = @_;
	if ($p == -1) {
		return $pkg;
	}
	if ($pkg =~ m/\-\d[^-]*/) {
		return "$`$&p$p$'";
	} else {
		return $pkg."p".$p;
	}
}

sub keep_most_recent
{
	my $h = {};
	for my $pkgname (@_) {
		my ($p, $v) = splitp($pkgname);
		if (!defined $h->{$p} || $h->{$p} < $v) {
			$h->{$p} = $v;
		}
	}
	my @list = ();
	while (my ($p, $v) = each %$h) {
		push(@list, rebuildp($p, $v));
	}
	return @list;
}

sub findstem
{
	my ($k, @list) = @_;
	my @r = ();
	for my $n (@list) {
		my $stem = splitstem($n);
		if ($k eq $stem) {
			push(@r, $n);
		}
	}
	return @r;
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

sub available_stems
{
	return avail2stems(OpenBSD::PackageLocator::available());
}

package OpenBSD::PackageLocator::_compiled_stemlist;

sub findstem
{
	my ($self, $stem) = @_;
	return keys %{$self->{$stem}};
}
	
package OpenBSD::PackageName::version;

sub make_dewey
{
	my $o = shift;
	$o->{deweys} = [ split(/\./, $o->{string}) ];
	for my $suffix (qw(rc beta pre pl)) {
		if ($o->{deweys}->[-1] =~ m/^(\d+)$suffix(\d*)$/) {
			$o->{deweys}->[-1] = $1;
			$o->{$suffix} = $2;
		}
	}
}

sub from_string
{
	my ($class, $string) = @_;
	my $vnum = -1;
	my $pnum = -1;
	if ($string =~ m/v(\d+)$/) {
		$vnum = $1;
		$string = $`;
	}
	if ($string =~ m/p(\d+)$/) {
		$pnum = $1;
		$string = $`;
	}
	my $o = bless {
		pnum => $pnum,
		vnum => $vnum,
		string => $string,
	}, $class;

	$o->make_dewey;
	return $o;
}

sub to_string
{
	my $o = shift;
	my $string = $o->{string};
	if ($o->{pnum} > -1) {
		$string .= 'p'.$o->{pnum};
	}
	if ($o->{vnum} > -1) {
		$string .= 'v'.$o->{vnum};
	}
	return $string;
}

sub compare
{
	my ($a, $b) = @_;
	# Simple case: epoch number
	if ($a->{vnum} != $b->{vnum}) {
		return $a->{vnum} <=> $b->{vnum};
	}
	# Simple case: only p number differs
	if ($a->{string} eq $b->{string}) {
		return $a->{pnum} <=> $b->{pnum}
	} 
	# Try a diff in dewey numbers first
	for (my $i = 0; ; $i++) {
		if (!defined $a->{deweys}->[$i]) {
			if (!defined $b->{deweys}->[$i]) {
				return 0;
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
	if ($a =~ m/^\d+$/ and $b =~ m/^\d+$/) {
		return $a <=> $b;
	}
	# added lowercase letter
	if ("$a.$b" =~ m/^(\d+)([a-z]?)\.(\d+)([a-z]?)$/) {
		my ($an, $al, $bn, $bl) = ($1, $2, $3, $4);
		if ($an != $bn) {
			return $an <=> $bn;
		} else {
			return $al cmp $bl;
		}
	}
	return $a cmp $b;
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
sub to_string
{
	my $o = shift;
	return join('-', $o->{stem}, $o->{version}->to_string, @{$o->{flavors}});
}

sub to_pattern
{
	my $o = shift;
	return join('-', $o->{stem}, '*', @{$o->{flavors}});
}

1;
