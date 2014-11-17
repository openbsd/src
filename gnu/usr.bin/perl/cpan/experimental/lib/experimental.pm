package experimental;
$experimental::VERSION = '0.007';
use strict;
use warnings;

use feature ();
use Carp qw/croak carp/;

my %warnings = map { $_ => 1 } grep { /^experimental::/ } keys %warnings::Offsets;
my %features = map { $_ => 1 } keys %feature::feature;

my %min_version = (
	array_base    => 5,
	autoderef     => 5.014000,
	lexical_topic => 5.010000,
	regex_sets    => 5.018000,
	smartmatch    => 5.010001,
	signatures    => 5.019009, # change to 5.20.0 someday? -- rjbs, 2014-02-08
);

my %additional = (
	postderef  => ['postderef_qq'],
	switch     => ['smartmatch'],
);

sub _enable {
	my $pragma = shift;
	if ($warnings{"experimental::$pragma"}) {
		warnings->unimport("experimental::$pragma");
		feature->import($pragma) if exists $features{$pragma};
		_enable(@{ $additional{$pragma} }) if $additional{$pragma};
	}
	elsif ($features{$pragma}) {
		feature->import($pragma);
		_enable(@{ $additional{$pragma} }) if $additional{$pragma};
	}
	elsif (not exists $min_version{$pragma}) {
		croak "Can't enable unknown feature $pragma";
	}
	elsif ($min_version{$pragma} > $]) {
		croak "Need perl version $min_version{$pragma} or later for feature $pragma";
	}
}

sub import {
	my ($self, @pragmas) = @_;

	for my $pragma (@pragmas) {
		_enable($pragma);
	}
	return;
}

sub _disable {
	my $pragma = shift;
	if ($warnings{"experimental::$pragma"}) {
		warnings->import("experimental::$pragma");
		feature->unimport($pragma) if exists $features{$pragma};
		_disable(@{ $additional{$pragma} }) if $additional{$pragma};
	}
	elsif ($features{$pragma}) {
		feature->unimport($pragma);
		_disable(@{ $additional{$pragma} }) if $additional{$pragma};
	}
	elsif (not exists $min_version{$pragma}) {
		carp "Can't disable unknown feature $pragma, ignoring";
	}
}

sub unimport {
	my ($self, @pragmas) = @_;

	for my $pragma (@pragmas) {
		_disable($pragma);
	}
	return;
}

1;

#ABSTRACT: Experimental features made easy

__END__

=pod

=encoding UTF-8

=head1 NAME

experimental - Experimental features made easy

=head1 VERSION

version 0.007

=head1 SYNOPSIS

 use experimental 'lexical_subs', 'smartmatch';
 my sub foo { $_[0] ~~ 1 }

=head1 DESCRIPTION

This pragma provides an easy and convenient way to enable or disable
experimental features.

Every version of perl has some number of features present but considered
"experimental."  For much of the life of Perl 5, this was only a designation
found in the documentation.  Starting in Perl v5.10.0, and more aggressively in
v5.18.0, experimental features were placed behind pragmata used to enable the
feature and disable associated warnings.

The C<experimental> pragma exists to combine the required incantations into a
single interface stable across releases of perl.  For every experimental
feature, this should enable the feature and silence warnings for the enclosing
lexical scope:

  use experimental 'feature-name';

To disable the feature and, if applicable, re-enable any warnings, use:

  no experimental 'feature-name';

The supported features, documented further below, are:

	array_base    - allow the use of $[ to change the starting index of @array
	autoderef     - allow push, each, keys, and other built-ins on references
	lexical_topic - allow the use of lexical $_ via "my $_"
	postderef     - allow the use of postfix dereferencing expressions, including
	                in interpolating strings
	regex_sets    - allow extended bracketed character classes in regexps
	signatures    - allow subroutine signatures (for named arguments)
	smartmatch    - allow the use of ~~, given, and when

=head2 Disclaimer

Because of the nature of the features it enables, forward compatibility can not
be guaranteed in any way.

=head1 AUTHOR

Leon Timmermans <leont@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2013 by Leon Timmermans.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
