package experimental;
$experimental::VERSION = '0.019';
use strict;
use warnings;
use version ();

BEGIN { eval { require feature } };
use Carp qw/croak carp/;

my %warnings = map { $_ => 1 } grep { /^experimental::/ } keys %warnings::Offsets;
my %features = map { $_ => 1 } $] > 5.015006 ? keys %feature::feature : do {
	my @features;
	if ($] >= 5.010) {
		push @features, qw/switch say state/;
		push @features, 'unicode_strings' if $] > 5.011002;
	}
	@features;
};

my %min_version = (
	array_base      => '5',
	autoderef       => '5.14.0',
	bitwise         => '5.22.0',
	const_attr      => '5.22.0',
	current_sub     => '5.16.0',
	evalbytes       => '5.16.0',
	fc              => '5.16.0',
	lexical_topic   => '5.10.0',
	lexical_subs    => '5.18.0',
	postderef       => '5.20.0',
	postderef_qq    => '5.20.0',
	refaliasing     => '5.22.0',
	regex_sets      => '5.18.0',
	say             => '5.10.0',
	smartmatch      => '5.10.0',
	signatures      => '5.20.0',
	state           => '5.10.0',
	switch          => '5.10.0',
	unicode_eval    => '5.16.0',
	unicode_strings => '5.12.0',
);
my %max_version = (
	autoderef       => '5.23.1',
	lexical_topic   => '5.23.4',
);

$_ = version->new($_) for values %min_version;
$_ = version->new($_) for values %max_version;

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
	elsif ($] < $min_version{$pragma}) {
		my $stable = $min_version{$pragma};
		if ($stable->{version}[1] % 2) {
			$stable = version->new(
				"5.".($stable->{version}[1]+1).'.0'
			);
		}
		croak "Need perl $stable or later for feature $pragma";
	}
	elsif ($] >= ($max_version{$pragma} || 7)) {
		croak "Experimental feature $pragma has been removed from perl in version $max_version{$pragma}";
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

version 0.019

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

=over 4

=item * C<array_base> - allow the use of C<$[> to change the starting index of C<@array>.

This is supported on all versions of perl.

=item * C<autoderef> - allow push, each, keys, and other built-ins on references.

This was added in perl 5.14.0 and removed in perl 5.23.1.

=item * C<bitwise> - allow the new stringwise bit operators

This was added in perl 5.22.0.

=item * C<const_attr> - allow the :const attribute on subs

This was added in perl 5.22.0.

=item * C<lexical_topic> - allow the use of lexical C<$_> via C<my $_>.

This was added in perl 5.10.0 and removed in perl 5.23.4.

=item * C<lexical_subs> - allow the use of lexical subroutines.

This was added in 5.18.0.

=item * C<postderef> - allow the use of postfix dereferencing expressions,
including in interpolating strings

This was added in perl 5.20.0.

=item * C<re_strict> - enables strict mode in regular expressions

This was added in perl 5.22.0.

=item * C<refaliasing> - allow aliasing via C<\$x = \$y>

This was added in perl 5.22.0.

=item * C<regex_sets> - allow extended bracketed character classes in regexps

This was added in perl 5.18.0.

=item * C<signatures> - allow subroutine signatures (for named arguments)

This was added in perl 5.20.0.

=item * C<smartmatch> - allow the use of C<~~>

This was added in perl 5.10.0, but it should be noted there are significant
incompatibilities between 5.10.0 and 5.10.1.

=item * C<switch> - allow the use of C<~~>, given, and when

This was added in perl 5.10.0.

=item * C<win32_perlio> - allows the use of the :win32 IO layer.

This was added on perl 5.22.0.

=back

=head2 Ordering matters

Using this pragma to 'enable an experimental feature' is another way of saying
that this pragma will disable the warnings which would result from using that
feature.  Therefore, the order in which pragmas are applied is important.  In
particular, you probably want to enable experimental features I<after> you
enable warnings:

  use warnings;
  use experimental 'smartmatch';

You also need to take care with modules that enable warnings for you.  A common
example being Moose.  In this example, warnings for the 'smartmatch' feature are
first turned on by the warnings pragma, off by the experimental pragma and back
on again by the Moose module (fix is to switch the last two lines):

  use warnings;
  use experimental 'smartmatch';
  use Moose;

=head2 Disclaimer

Because of the nature of the features it enables, forward compatibility can not
be guaranteed in any way.

=head1 SEE ALSO

L<perlexperimental|perlexperimental> contains more information about experimental features.

=head1 AUTHOR

Leon Timmermans <leont@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2013 by Leon Timmermans.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
