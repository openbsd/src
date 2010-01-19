# ex:ts=8 sw=4:
# $OpenBSD: LibSpec.pm,v 1.2 2010/01/19 14:58:53 espie Exp $
#
# Copyright (c) 2010 Marc Espie <espie@openbsd.org>
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
#
use strict;
use warnings;

package OpenBSD::Library;

package OpenBSD::LibSpec;

sub new
{
	my ($class, $dir, $stem, $major, $minor) = @_;
	$dir //= "lib";
	bless { 
		dir => $dir, stem => $stem, 
		major => $major, minor => $minor 
	    }, $class;
}

sub key
{
	my $self = shift;
	return "$self->{dir}/$self->{stem}";
}

sub major
{
	my $self = shift;
	return $self->{major};
}

sub minor
{
	my $self = shift;
	return $self->{minor};
}

sub badspec
{
	"OpenBSD::LibSpec::BadSpec";
}

my $cached = {};

sub from_string
{
	my ($class, $_) = @_;
	return $cached->{$_} //= $class->new_from_string($_);
}

sub new_from_string
{
	my ($class, $string) = @_;
	if (my ($stem, $major, $minor) = $string =~ m/^(.*)\.(\d+)\.(\d+)$/o) {
		if ($stem =~ m/^(.*)\/([^\/]+)$/o) {
			return $class->new($1, $2, $major, $minor);
		} else {
			return $class->new(undef, $stem, $major, $minor);
		}
	} else {
		return $class->badspec->new($string);
	}
}

sub to_string
{
	my $self = shift;
	my $s = join('.', $self->{stem}, $self->{major}, $self->{minor});

	if ($self->{dir} ne 'lib') {
		$s = "$self->{dir}/$s";
	}
	return $s;
}

sub is_valid
{
	return 1;
}
package OpenBSD::LibSpec::BadSpec;
our @ISA=qw(OpenBSD::LibSpec);

sub to_string
{
	my $self = shift;
	return $$self;
}

sub new
{
	my ($class, $string) = @_;
	bless \$string, $class;
}

sub is_valid
{
	return 0;
}

1;
