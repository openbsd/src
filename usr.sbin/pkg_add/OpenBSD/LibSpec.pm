# ex:ts=8 sw=4:
# $OpenBSD: LibSpec.pm,v 1.5 2010/03/22 20:38:44 espie Exp $
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

package OpenBSD::LibObject;

sub key
{
	my $self = shift;
	if (defined $self->{dir}) {
		return "$self->{dir}/$self->{stem}";
	} else {
		return $self->{stem};
	}
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

sub is_valid
{
	return 1;
}

sub stem
{
	my $self = shift;
	return $self->{stem};
}

sub badclass
{
	"OpenBSD::BadLib";
}

sub lookup
{
	my ($spec, $repo, $base) = @_;

	my $approx = $spec->lookup_stem($repo);
	if (!defined $approx) {
		return undef;
	}
	my $r = [];
	for my $c (@$approx) {
		if ($spec->match($c, $base)) {
			push(@$r, $c);
		}
	}
	return $r;
}

package OpenBSD::BadLib;
our @ISA=qw(OpenBSD::LibObject);

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

sub lookup_stem
{
	return undef;
}

sub match
{
	return 0;
}

package OpenBSD::LibRepo;
sub new
{
	my $class = shift;
	bless {}, $class;
}

sub register
{
	my ($repo, $lib, $origin) = @_;
	$lib->set_origin($origin);
	push @{$repo->{$lib->stem}}, $lib;
}



package OpenBSD::Library;
our @ISA = qw(OpenBSD::LibObject);

sub from_string
{
	my ($class, $filename) = @_;
	if (my ($dir, $stem, $major, $minor) = $filename =~ m/^(.*)\/lib([^\/]+)\.so\.(\d+)\.(\d+)$/o) {
		bless { dir => $dir, stem => $stem, major => $major, 
		    minor => $minor }, $class;
	} else {
		return $class->badclass->new($filename);
	}
}

sub to_string
{
	my $self = shift;
	return "$self->{dir}/lib$self->{stem}.so.$self->{major}.$self->{minor}";
}

sub set_origin
{
	my ($self, $origin) = @_;
	$self->{origin} = $origin;
	return $self;
}

sub origin
{
	my $self = shift;
	return $self->{origin};
}

package OpenBSD::LibSpec;
our @ISA = qw(OpenBSD::LibObject);

sub new
{
	my ($class, $dir, $stem, $major, $minor) = @_;
	bless { 
		dir => $dir, stem => $stem, 
		major => $major, minor => $minor 
	    }, $class;
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
		return $class->badclass->new($string);
	}
}

sub to_string
{
	my $self = shift;
	return join('.', $self->key, $self->major, $self->minor);

}

sub lookup_stem
{
	my ($spec, $repo) = @_;

	my $result = $repo->{$spec->stem};
	if (!defined $result) {
		return undef;
	} else {
		return $result;
	}
}

sub no_match
{
	my ($spec, $library, $base) = @_;
	if ($spec->major != $library->major) {
		return "bad major";
	}
	if ($spec->minor > $library->minor) {
		return "minor is too small";
	}
	if (defined $spec->{dir}) {
		if ("$base/$spec->{dir}" eq $library->{dir}) {
			return undef;
		}
	} else {
		for my $d ($base, OpenBSD::Paths->library_dirs) {
			if ("$d/lib" eq $library->{dir}) {
				return undef;
			}
		}
	}
	return "bad directory";
}
sub match
{
	my ($spec, $library, $base) = @_;
	return !$spec->no_match($library, $base);
}

1;
