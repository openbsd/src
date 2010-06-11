# ex:ts=8 sw=4:
# $OpenBSD: PkgSpec.pm,v 1.29 2010/06/11 09:56:44 espie Exp $
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

package OpenBSD::PkgSpec::flavorspec;
sub new
{
	my ($class, $spec) = @_;

	$spec =~ s/^-//o;

	bless \$spec, $class;
}

sub check_1flavor
{
	my ($f, $spec) = @_;

	for my $_ (split /\-/o, $spec) {
		# must not be here
		if (m/^\!(.*)$/o) {
			return 0 if $f->{$1};
		# must be here
		} else {
			return 0 unless $f->{$_};
		}
	}
	return 1;
}

sub match
{
	my ($self, $h) = @_;

	# check each flavor constraint
	for my $_ (split /\,/o, $$self) {
		if (check_1flavor($h->{flavors}, $_)) {
			return 1;
		}
	}
	return 0;
}

package OpenBSD::PkgSpec::exactflavor;
our @ISA = qw(OpenBSD::PkgSpec::flavorspec);
sub match
{
	my ($self, $h) = @_;
	if ($$self eq $h->flavor_string) {
		return 1;
	} else {
		return 0;
	}
}

package OpenBSD::PkgSpec::versionspec;
sub new
{
	my ($class, $s) = @_;
	my $spec = OpenBSD::PackageName::versionspec->from_string($s);
	bless \$spec, $class;
}

sub match
{
	my ($self, $name) = @_;

	return $$self->match($name->{version});
}

package OpenBSD::PkgSpec::badspec;
sub new
{
	my $class = shift;
	bless {}, $class;
}

sub match_ref
{
	return ();
}

sub match_locations
{
	return ();
}

sub is_valid
{
	return 0;
}

package OpenBSD::PkgSpec::SubPattern;
use OpenBSD::PackageName;

my $exception = {
	"db-3.*" => "db->=3,<4",
	"db-4.*" => "db->=4,<5",
	"db-java-4.*" => "db-java->=4,<5",
	"emacs-21.*" => "emacs->=21,<22",
	"emacs-21.4*" => "emacs->=21.4,<21.5",
	"emacs-22.2*" => "emacs->=22.2,<22.3",
	"enlightenment-0.16*" => "enlightenment->=0.16,<0.17",
	"gimp-2.*" => "gimp->=2,<3",
	"gnupg->=1.4.*" => "gnupg->=1.4",
	"gstreamer-0.10.*" => "gstreamer->=0.10,<0.11",
	"gtksourceview-2.*" => "gtksourceview->=2,<3",
	"hydra-5.4*" => "hydra->=5.4,<5.5",
	"jdk->=1.5.0.*" => "jdk->=1.5.0",
	"jdk->=1.6.0.*" => "jdk->=1.6.0",
	"jre->=1.5.0.*" => "jre->=1.5.0",
	"libggi->=0.9*" => "libggi->=0.9",
	"libnet-1.0*" => "libnet->=1.0,<1.1",
	"libnet-1.0.*" => "libnet->=1.0,<1.1",
	"libnet-1.1*" => "libnet->=1.1,<1.2",
	"libsigc++-1.*" => "libsigc++->=1,<2",
	"libsigc++-2.*" => "libsigc++->=2,<3",
	"mysql-client-5.0.*" => "mysql-client->=5.0,<5.1",
	"ocaml-3.09.3*" => "ocaml->=3.09.3,<3.09.4",
	"openldap-client-2.*" => "openldap-client->=2,<3",
	"pgp-5.*" => "pgp->=5,<6",
	"postgresql-client-8.3.*" => "postgresql-client->=8.3,<8.4",
	"python-2.4*" => "python->=2.4,<2.5",
	"python-2.4.*" => "python->=2.4,<2.5",
	"python-2.5*" => "python->=2.5,<2.6",
	"python-2.5.*" => "python->=2.5,<2.6",
	"python-2.6.*" => "python->=2.6,<2.7",
	"python-bsddb-2.5*" => "python-bsddb->=2.5,<2.6",
	"python-tkinter-2.4*" => "python-tkinter->=2.4,<2.5",
	"python-tkinter-2.5*" => "python-tkinter->=2.5,<2.6",
	"rrdtool-1.2.*" => "rrdtool->=1.2,<1.3",
	"swt-3.2.2*" => "swt->=3.2.2,<3.2.3",
	"swt-browser-3.2.2*" => "swt-browser->=3.2.2,<3.2.3",
	"tcl-8.4.*" => "tcl->=8.4,<8.5",
	"tcl-8.5.*" => "tcl->=8.5,<8.6",
	"tk-8.4*" => "tk->=8.4,<8.5",
	"tk-8.4.*" => "tk->=8.4,<8.5",
	"tk-8.5*" => "tk->=8.5,<8.6",
	"tomcat-4.*" => "tomcat->=4,<5",
	"tomcat-5.*" => "tomcat->=5,<6",
	"tomcat-6.*" => "tomcat->=6,<7",
	"tomcat-admin-4.*" => "tomcat-admin->=4,<5",
	"tomcat-admin-5.*" => "tomcat-admin->=5,<6",
	"xmms-1.2.11*" => "xmms->=1.2.11,<1.2.12"
};

sub parse
{
	my ($class, $p) = @_;

	my $r = {};

	if (defined $exception->{$p}) {
		$p = $exception->{$p};
		$r->{e} = 1;
	}

	# let's try really hard to find the stem and the flavors
	unless ($p =~ m/^(.*?)\-((?:(?:\>|\>\=|\<\=|\<|\=)?\d|\*)[^-]*)(.*)$/) {
		return undef;
	}
	($r->{stemspec}, $r->{vspec}, $r->{flavorspec}) = ($1, $2, $3);

	$r->{stemspec} =~ s/\./\\\./go;
	$r->{stemspec} =~ s/\+/\\\+/go;
	$r->{stemspec} =~ s/\*/\.\*/go;
	$r->{stemspec} =~ s/\?/\./go;
	$r->{stemspec} =~ s/^(\\\.libs)\-/$1\\d*\-/go;
	return $r;
}

sub add_version_constraints
{
	my ($class, $constraints, $vspec) = @_;

	# turn the vspec into a list of constraints.
	if ($vspec eq '*') {
		# non constraint
	} else {
		for my $c (split /\,/, $vspec) {
			push(@$constraints,
			    OpenBSD::PkgSpec::versionspec->new($c));
		}
	}
}

sub add_flavor_constraints
{
	my ($class, $constraints, $flavorspec) = @_;
	# and likewise for flavors
	if ($flavorspec eq '') {
		# non constraint
	} else {
		push(@$constraints,
		    OpenBSD::PkgSpec::flavorspec->new($flavorspec));
	}
}

sub new
{
	my ($class, $p) = @_;

	my $r = $class->parse($p);
	if (defined $r) {
		my $stemspec = $r->{stemspec};
		my $constraints = [];
		$class->add_version_constraints($constraints, $r->{vspec});
		$class->add_flavor_constraints($constraints, $r->{flavorspec});

		my $o = bless {
			exactstem => qr{^$stemspec$},
			fuzzystem => qr{^$stemspec\-\d.*$},
			constraints => $constraints,
		    }, $class;
		if (defined $r->{e}) {
			$o->{e} = 1;
		}
	   	return $o;
	} else {
		return OpenBSD::PkgSpec::badspec->new;
	}
}

sub match_ref
{
	my ($o, $list) = @_;
	my @result = ();
	# Now, have to extract the version number, and the flavor...
LOOP1:
	for my $s (grep(/$o->{fuzzystem}/, @$list)) {
		my $name = OpenBSD::PackageName->from_string($s);
		next unless $name->{stem} =~ m/^$o->{exactstem}$/;
		for my $c (@{$o->{constraints}}) {
			next LOOP1 unless $c->match($name);
		}
		if (wantarray) {
			push(@result, $s);
		} else {
			return 1;
		}
	}

	return @result;
}

sub match_locations
{
	my ($o, $list) = @_;
	my $result = [];
	# Now, have to extract the version number, and the flavor...
LOOP2:
	for my $s (grep { $_->name =~ m/$o->{fuzzystem}/} @$list) {
		my $name = $s->pkgname;
		next unless $name->{stem} =~ m/^$o->{exactstem}$/;
		for my $c (@{$o->{constraints}}) {
			next LOOP2 unless $c->match($name);
		}
		push(@$result, $s);
	}

	return $result;
}

sub is_valid
{
	return !defined shift->{e};
}

package OpenBSD::PkgSpec;
sub subpattern_class
{ "OpenBSD::PkgSpec::SubPattern" }
sub new
{
	my ($class, $pattern) = @_;
	my @l = map { $class->subpattern_class->new($_) }
		(split /\|/o, $pattern);
	if (@l == 1) {
		return $l[0];
	} else {
		return bless \@l, $class;
	}
}

sub match_ref
{
	my ($self, $r) = @_;
	if (wantarray) {
		my @l = ();
		for my $subpattern (@$self) {
			push(@l, $subpattern->match_ref($r));
		}
		return @l;
	} else {
		for my $subpattern (@$self) {
			if ($subpattern->match_ref($r)) {
				return 1;
			}
		}
		return 0;
	}
}

sub match_locations
{
	my ($self, $r) = @_;
	my $l = [];
	for my $subpattern (@$self) {
		push(@$l, @{$subpattern->match_locations($r)});
	}
	return $l;
}

sub is_valid
{
	my $self = shift;
	for my $subpattern (@$self) {
		return 0 unless $subpattern->is_valid;
	}
	return 1;
}

package OpenBSD::PkgSpec::SubPattern::Exact;
our @ISA = qw(OpenBSD::PkgSpec::SubPattern);

sub add_version_constraints
{
	my ($class, $constraints, $vspec) = @_;
	return if $vspec eq '*'; # XXX
	my $v = OpenBSD::PkgSpec::versionspec->new($vspec);
	die "not a good exact spec" if !$$v->is_exact;
	delete $$v->{p};
	push(@$constraints, $v);
}

sub add_flavor_constraints
{
	my ($class, $constraints, $flavorspec) = @_;
	push(@$constraints, OpenBSD::PkgSpec::exactflavor->new($flavorspec));
}

package OpenBSD::PkgSpec::Exact;
our @ISA = qw(OpenBSD::PkgSpec);

sub subpattern_class
{ "OpenBSD::PkgSpec::SubPattern::Exact" }

1;
