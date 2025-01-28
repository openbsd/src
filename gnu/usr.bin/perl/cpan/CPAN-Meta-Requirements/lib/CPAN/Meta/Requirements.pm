use v5.10;
use strict;
use warnings;
package CPAN::Meta::Requirements;
# ABSTRACT: a set of version requirements for a CPAN dist

our $VERSION = '2.143';

use CPAN::Meta::Requirements::Range;

#pod =head1 SYNOPSIS
#pod
#pod   use CPAN::Meta::Requirements;
#pod
#pod   my $build_requires = CPAN::Meta::Requirements->new;
#pod
#pod   $build_requires->add_minimum('Library::Foo' => 1.208);
#pod
#pod   $build_requires->add_minimum('Library::Foo' => 2.602);
#pod
#pod   $build_requires->add_minimum('Module::Bar'  => 'v1.2.3');
#pod
#pod   $METAyml->{build_requires} = $build_requires->as_string_hash;
#pod
#pod =head1 DESCRIPTION
#pod
#pod A CPAN::Meta::Requirements object models a set of version constraints like
#pod those specified in the F<META.yml> or F<META.json> files in CPAN distributions,
#pod and as defined by L<CPAN::Meta::Spec>.
#pod It can be built up by adding more and more constraints, and it will reduce them
#pod to the simplest representation.
#pod
#pod Logically impossible constraints will be identified immediately by thrown
#pod exceptions.
#pod
#pod =cut

use Carp ();

#pod =method new
#pod
#pod   my $req = CPAN::Meta::Requirements->new;
#pod
#pod This returns a new CPAN::Meta::Requirements object.  It takes an optional
#pod hash reference argument.  Currently, only one key is supported:
#pod
#pod =for :list
#pod * C<bad_version_hook> -- if provided, when a version cannot be parsed into
#pod   a version object, this code reference will be called with the invalid
#pod   version string as first argument, and the module name as second
#pod   argument.  It must return a valid version object.
#pod
#pod All other keys are ignored.
#pod
#pod =cut

my @valid_options = qw( bad_version_hook );

sub new {
  my ($class, $options) = @_;
  $options ||= {};
  Carp::croak "Argument to $class\->new() must be a hash reference"
    unless ref $options eq 'HASH';
  my %self = map {; $_ => $options->{$_}} @valid_options;

  return bless \%self => $class;
}

#pod =method add_minimum
#pod
#pod   $req->add_minimum( $module => $version );
#pod
#pod This adds a new minimum version requirement.  If the new requirement is
#pod redundant to the existing specification, this has no effect.
#pod
#pod Minimum requirements are inclusive.  C<$version> is required, along with any
#pod greater version number.
#pod
#pod This method returns the requirements object.
#pod
#pod =method add_maximum
#pod
#pod   $req->add_maximum( $module => $version );
#pod
#pod This adds a new maximum version requirement.  If the new requirement is
#pod redundant to the existing specification, this has no effect.
#pod
#pod Maximum requirements are inclusive.  No version strictly greater than the given
#pod version is allowed.
#pod
#pod This method returns the requirements object.
#pod
#pod =method add_exclusion
#pod
#pod   $req->add_exclusion( $module => $version );
#pod
#pod This adds a new excluded version.  For example, you might use these three
#pod method calls:
#pod
#pod   $req->add_minimum( $module => '1.00' );
#pod   $req->add_maximum( $module => '1.82' );
#pod
#pod   $req->add_exclusion( $module => '1.75' );
#pod
#pod Any version between 1.00 and 1.82 inclusive would be acceptable, except for
#pod 1.75.
#pod
#pod This method returns the requirements object.
#pod
#pod =method exact_version
#pod
#pod   $req->exact_version( $module => $version );
#pod
#pod This sets the version required for the given module to I<exactly> the given
#pod version.  No other version would be considered acceptable.
#pod
#pod This method returns the requirements object.
#pod
#pod =cut

BEGIN {
  for my $type (qw(maximum exclusion exact_version)) {
    my $method = "with_$type";
    my $to_add = $type eq 'exact_version' ? $type : "add_$type";

    my $code = sub {
      my ($self, $name, $version) = @_;

      $self->__modify_entry_for($name, $method, $version);

      return $self;
    };

    no strict 'refs';
    *$to_add = $code;
  }
}

# add_minimum is optimized compared to generated subs above because
# it is called frequently and with "0" or equivalent input
sub add_minimum {
  my ($self, $name, $version) = @_;

  # stringify $version so that version->new("0.00")->stringify ne "0"
  # which preserves the user's choice of "0.00" as the requirement
  if (not defined $version or "$version" eq '0') {
    return $self if $self->__entry_for($name);
    Carp::croak("can't add new requirements to finalized requirements")
      if $self->is_finalized;

    $self->{requirements}{ $name } =
      CPAN::Meta::Requirements::Range->with_minimum('0', $name);
  }
  else {
    $self->__modify_entry_for($name, 'with_minimum', $version);
  }
  return $self;
}

#pod =method version_range_for_module
#pod
#pod   $req->version_range_for_module( $another_req_object );
#pod
#pod =cut

sub version_range_for_module {
  my ($self, $module) = @_;
  return $self->{requirements}{$module};
}

#pod =method add_requirements
#pod
#pod   $req->add_requirements( $another_req_object );
#pod
#pod This method adds all the requirements in the given CPAN::Meta::Requirements
#pod object to the requirements object on which it was called.  If there are any
#pod conflicts, an exception is thrown.
#pod
#pod This method returns the requirements object.
#pod
#pod =cut

sub add_requirements {
  my ($self, $req) = @_;

  for my $module ($req->required_modules) {
    my $new_range = $req->version_range_for_module($module);
    $self->__modify_entry_for($module, 'with_range', $new_range);
  }

  return $self;
}

#pod =method accepts_module
#pod
#pod   my $bool = $req->accepts_module($module => $version);
#pod
#pod Given an module and version, this method returns true if the version
#pod specification for the module accepts the provided version.  In other words,
#pod given:
#pod
#pod   Module => '>= 1.00, < 2.00'
#pod
#pod We will accept 1.00 and 1.75 but not 0.50 or 2.00.
#pod
#pod For modules that do not appear in the requirements, this method will return
#pod true.
#pod
#pod =cut

sub accepts_module {
  my ($self, $module, $version) = @_;

  return 1 unless my $range = $self->__entry_for($module);
  return $range->accepts($version);
}

#pod =method clear_requirement
#pod
#pod   $req->clear_requirement( $module );
#pod
#pod This removes the requirement for a given module from the object.
#pod
#pod This method returns the requirements object.
#pod
#pod =cut

sub clear_requirement {
  my ($self, $module) = @_;

  return $self unless $self->__entry_for($module);

  Carp::croak("can't clear requirements on finalized requirements")
    if $self->is_finalized;

  delete $self->{requirements}{ $module };

  return $self;
}

#pod =method requirements_for_module
#pod
#pod   $req->requirements_for_module( $module );
#pod
#pod This returns a string containing the version requirements for a given module in
#pod the format described in L<CPAN::Meta::Spec> or undef if the given module has no
#pod requirements. This should only be used for informational purposes such as error
#pod messages and should not be interpreted or used for comparison (see
#pod L</accepts_module> instead).
#pod
#pod =cut

sub requirements_for_module {
  my ($self, $module) = @_;
  my $entry = $self->__entry_for($module);
  return unless $entry;
  return $entry->as_string;
}

#pod =method structured_requirements_for_module
#pod
#pod   $req->structured_requirements_for_module( $module );
#pod
#pod This returns a data structure containing the version requirements for a given
#pod module or undef if the given module has no requirements.  This should
#pod not be used for version checks (see L</accepts_module> instead).
#pod
#pod Added in version 2.134.
#pod
#pod =cut

sub structured_requirements_for_module {
  my ($self, $module) = @_;
  my $entry = $self->__entry_for($module);
  return unless $entry;
  return $entry->as_struct;
}

#pod =method required_modules
#pod
#pod This method returns a list of all the modules for which requirements have been
#pod specified.
#pod
#pod =cut

sub required_modules { keys %{ $_[0]{requirements} } }

#pod =method clone
#pod
#pod   $req->clone;
#pod
#pod This method returns a clone of the invocant.  The clone and the original object
#pod can then be changed independent of one another.
#pod
#pod =cut

sub clone {
  my ($self) = @_;
  my $new = (ref $self)->new;

  return $new->add_requirements($self);
}

sub __entry_for     { $_[0]{requirements}{ $_[1] } }

sub __modify_entry_for {
  my ($self, $name, $method, $version) = @_;

  my $fin = $self->is_finalized;
  my $old = $self->__entry_for($name);

  Carp::croak("can't add new requirements to finalized requirements")
    if $fin and not $old;

  my $new = ($old || 'CPAN::Meta::Requirements::Range')
          ->$method($version, $name, $self->{bad_version_hook});

  Carp::croak("can't modify finalized requirements")
    if $fin and $old->as_string ne $new->as_string;

  $self->{requirements}{ $name } = $new;
}

#pod =method is_simple
#pod
#pod This method returns true if and only if all requirements are inclusive minimums
#pod -- that is, if their string expression is just the version number.
#pod
#pod =cut

sub is_simple {
  my ($self) = @_;
  for my $module ($self->required_modules) {
    # XXX: This is a complete hack, but also entirely correct.
    return if not $self->__entry_for($module)->is_simple;
  }

  return 1;
}

#pod =method is_finalized
#pod
#pod This method returns true if the requirements have been finalized by having the
#pod C<finalize> method called on them.
#pod
#pod =cut

sub is_finalized { $_[0]{finalized} }

#pod =method finalize
#pod
#pod This method marks the requirements finalized.  Subsequent attempts to change
#pod the requirements will be fatal, I<if> they would result in a change.  If they
#pod would not alter the requirements, they have no effect.
#pod
#pod If a finalized set of requirements is cloned, the cloned requirements are not
#pod also finalized.
#pod
#pod =cut

sub finalize { $_[0]{finalized} = 1 }

#pod =method as_string_hash
#pod
#pod This returns a reference to a hash describing the requirements using the
#pod strings in the L<CPAN::Meta::Spec> specification.
#pod
#pod For example after the following program:
#pod
#pod   my $req = CPAN::Meta::Requirements->new;
#pod
#pod   $req->add_minimum('CPAN::Meta::Requirements' => 0.102);
#pod
#pod   $req->add_minimum('Library::Foo' => 1.208);
#pod
#pod   $req->add_maximum('Library::Foo' => 2.602);
#pod
#pod   $req->add_minimum('Module::Bar'  => 'v1.2.3');
#pod
#pod   $req->add_exclusion('Module::Bar'  => 'v1.2.8');
#pod
#pod   $req->exact_version('Xyzzy'  => '6.01');
#pod
#pod   my $hashref = $req->as_string_hash;
#pod
#pod C<$hashref> would contain:
#pod
#pod   {
#pod     'CPAN::Meta::Requirements' => '0.102',
#pod     'Library::Foo' => '>= 1.208, <= 2.206',
#pod     'Module::Bar'  => '>= v1.2.3, != v1.2.8',
#pod     'Xyzzy'        => '== 6.01',
#pod   }
#pod
#pod =cut

sub as_string_hash {
  my ($self) = @_;

  my %hash = map {; $_ => $self->{requirements}{$_}->as_string }
             $self->required_modules;

  return \%hash;
}

#pod =method add_string_requirement
#pod
#pod   $req->add_string_requirement('Library::Foo' => '>= 1.208, <= 2.206');
#pod   $req->add_string_requirement('Library::Foo' => v1.208);
#pod
#pod This method parses the passed in string and adds the appropriate requirement
#pod for the given module.  A version can be a Perl "v-string".  It understands
#pod version ranges as described in the L<CPAN::Meta::Spec/Version Ranges>. For
#pod example:
#pod
#pod =over 4
#pod
#pod =item 1.3
#pod
#pod =item >= 1.3
#pod
#pod =item <= 1.3
#pod
#pod =item == 1.3
#pod
#pod =item != 1.3
#pod
#pod =item > 1.3
#pod
#pod =item < 1.3
#pod
#pod =item >= 1.3, != 1.5, <= 2.0
#pod
#pod A version number without an operator is equivalent to specifying a minimum
#pod (C<E<gt>=>).  Extra whitespace is allowed.
#pod
#pod =back
#pod
#pod =cut

sub add_string_requirement {
  my ($self, $module, $req) = @_;

  $self->__modify_entry_for($module, 'with_string_requirement', $req);
}

#pod =method from_string_hash
#pod
#pod   my $req = CPAN::Meta::Requirements->from_string_hash( \%hash );
#pod   my $req = CPAN::Meta::Requirements->from_string_hash( \%hash, \%opts );
#pod
#pod This is an alternate constructor for a CPAN::Meta::Requirements
#pod object. It takes a hash of module names and version requirement
#pod strings and returns a new CPAN::Meta::Requirements object. As with
#pod add_string_requirement, a version can be a Perl "v-string". Optionally,
#pod you can supply a hash-reference of options, exactly as with the L</new>
#pod method.
#pod
#pod =cut

sub from_string_hash {
  my ($class, $hash, $options) = @_;

  my $self = $class->new($options);

  for my $module (keys %$hash) {
    my $req = $hash->{$module};
    $self->add_string_requirement($module, $req);
  }

  return $self;
}

1;
# vim: ts=2 sts=2 sw=2 et:

__END__

=pod

=encoding UTF-8

=head1 NAME

CPAN::Meta::Requirements - a set of version requirements for a CPAN dist

=head1 VERSION

version 2.143

=head1 SYNOPSIS

  use CPAN::Meta::Requirements;

  my $build_requires = CPAN::Meta::Requirements->new;

  $build_requires->add_minimum('Library::Foo' => 1.208);

  $build_requires->add_minimum('Library::Foo' => 2.602);

  $build_requires->add_minimum('Module::Bar'  => 'v1.2.3');

  $METAyml->{build_requires} = $build_requires->as_string_hash;

=head1 DESCRIPTION

A CPAN::Meta::Requirements object models a set of version constraints like
those specified in the F<META.yml> or F<META.json> files in CPAN distributions,
and as defined by L<CPAN::Meta::Spec>.
It can be built up by adding more and more constraints, and it will reduce them
to the simplest representation.

Logically impossible constraints will be identified immediately by thrown
exceptions.

=head1 METHODS

=head2 new

  my $req = CPAN::Meta::Requirements->new;

This returns a new CPAN::Meta::Requirements object.  It takes an optional
hash reference argument.  Currently, only one key is supported:

=over 4

=item *

C<bad_version_hook> -- if provided, when a version cannot be parsed into a version object, this code reference will be called with the invalid version string as first argument, and the module name as second argument.  It must return a valid version object.

=back

All other keys are ignored.

=head2 add_minimum

  $req->add_minimum( $module => $version );

This adds a new minimum version requirement.  If the new requirement is
redundant to the existing specification, this has no effect.

Minimum requirements are inclusive.  C<$version> is required, along with any
greater version number.

This method returns the requirements object.

=head2 add_maximum

  $req->add_maximum( $module => $version );

This adds a new maximum version requirement.  If the new requirement is
redundant to the existing specification, this has no effect.

Maximum requirements are inclusive.  No version strictly greater than the given
version is allowed.

This method returns the requirements object.

=head2 add_exclusion

  $req->add_exclusion( $module => $version );

This adds a new excluded version.  For example, you might use these three
method calls:

  $req->add_minimum( $module => '1.00' );
  $req->add_maximum( $module => '1.82' );

  $req->add_exclusion( $module => '1.75' );

Any version between 1.00 and 1.82 inclusive would be acceptable, except for
1.75.

This method returns the requirements object.

=head2 exact_version

  $req->exact_version( $module => $version );

This sets the version required for the given module to I<exactly> the given
version.  No other version would be considered acceptable.

This method returns the requirements object.

=head2 version_range_for_module

  $req->version_range_for_module( $another_req_object );

=head2 add_requirements

  $req->add_requirements( $another_req_object );

This method adds all the requirements in the given CPAN::Meta::Requirements
object to the requirements object on which it was called.  If there are any
conflicts, an exception is thrown.

This method returns the requirements object.

=head2 accepts_module

  my $bool = $req->accepts_module($module => $version);

Given an module and version, this method returns true if the version
specification for the module accepts the provided version.  In other words,
given:

  Module => '>= 1.00, < 2.00'

We will accept 1.00 and 1.75 but not 0.50 or 2.00.

For modules that do not appear in the requirements, this method will return
true.

=head2 clear_requirement

  $req->clear_requirement( $module );

This removes the requirement for a given module from the object.

This method returns the requirements object.

=head2 requirements_for_module

  $req->requirements_for_module( $module );

This returns a string containing the version requirements for a given module in
the format described in L<CPAN::Meta::Spec> or undef if the given module has no
requirements. This should only be used for informational purposes such as error
messages and should not be interpreted or used for comparison (see
L</accepts_module> instead).

=head2 structured_requirements_for_module

  $req->structured_requirements_for_module( $module );

This returns a data structure containing the version requirements for a given
module or undef if the given module has no requirements.  This should
not be used for version checks (see L</accepts_module> instead).

Added in version 2.134.

=head2 required_modules

This method returns a list of all the modules for which requirements have been
specified.

=head2 clone

  $req->clone;

This method returns a clone of the invocant.  The clone and the original object
can then be changed independent of one another.

=head2 is_simple

This method returns true if and only if all requirements are inclusive minimums
-- that is, if their string expression is just the version number.

=head2 is_finalized

This method returns true if the requirements have been finalized by having the
C<finalize> method called on them.

=head2 finalize

This method marks the requirements finalized.  Subsequent attempts to change
the requirements will be fatal, I<if> they would result in a change.  If they
would not alter the requirements, they have no effect.

If a finalized set of requirements is cloned, the cloned requirements are not
also finalized.

=head2 as_string_hash

This returns a reference to a hash describing the requirements using the
strings in the L<CPAN::Meta::Spec> specification.

For example after the following program:

  my $req = CPAN::Meta::Requirements->new;

  $req->add_minimum('CPAN::Meta::Requirements' => 0.102);

  $req->add_minimum('Library::Foo' => 1.208);

  $req->add_maximum('Library::Foo' => 2.602);

  $req->add_minimum('Module::Bar'  => 'v1.2.3');

  $req->add_exclusion('Module::Bar'  => 'v1.2.8');

  $req->exact_version('Xyzzy'  => '6.01');

  my $hashref = $req->as_string_hash;

C<$hashref> would contain:

  {
    'CPAN::Meta::Requirements' => '0.102',
    'Library::Foo' => '>= 1.208, <= 2.206',
    'Module::Bar'  => '>= v1.2.3, != v1.2.8',
    'Xyzzy'        => '== 6.01',
  }

=head2 add_string_requirement

  $req->add_string_requirement('Library::Foo' => '>= 1.208, <= 2.206');
  $req->add_string_requirement('Library::Foo' => v1.208);

This method parses the passed in string and adds the appropriate requirement
for the given module.  A version can be a Perl "v-string".  It understands
version ranges as described in the L<CPAN::Meta::Spec/Version Ranges>. For
example:

=over 4

=item 1.3

=item >= 1.3

=item <= 1.3

=item == 1.3

=item != 1.3

=item > 1.3

=item < 1.3

=item >= 1.3, != 1.5, <= 2.0

A version number without an operator is equivalent to specifying a minimum
(C<E<gt>=>).  Extra whitespace is allowed.

=back

=head2 from_string_hash

  my $req = CPAN::Meta::Requirements->from_string_hash( \%hash );
  my $req = CPAN::Meta::Requirements->from_string_hash( \%hash, \%opts );

This is an alternate constructor for a CPAN::Meta::Requirements
object. It takes a hash of module names and version requirement
strings and returns a new CPAN::Meta::Requirements object. As with
add_string_requirement, a version can be a Perl "v-string". Optionally,
you can supply a hash-reference of options, exactly as with the L</new>
method.

=for :stopwords cpan testmatrix url bugtracker rt cpants kwalitee diff irc mailto metadata placeholders metacpan

=head1 SUPPORT

=head2 Bugs / Feature Requests

Please report any bugs or feature requests through the issue tracker
at L<https://github.com/Perl-Toolchain-Gang/CPAN-Meta-Requirements/issues>.
You will be notified automatically of any progress on your issue.

=head2 Source Code

This is open source software.  The code repository is available for
public review and contribution under the terms of the license.

L<https://github.com/Perl-Toolchain-Gang/CPAN-Meta-Requirements>

  git clone https://github.com/Perl-Toolchain-Gang/CPAN-Meta-Requirements.git

=head1 AUTHORS

=over 4

=item *

David Golden <dagolden@cpan.org>

=item *

Ricardo Signes <rjbs@cpan.org>

=back

=head1 CONTRIBUTORS

=for stopwords Ed J Graham Knop Karen Etheridge Leon Timmermans Paul Howarth Ricardo Signes robario Tatsuhiko Miyagawa

=over 4

=item *

Ed J <mohawk2@users.noreply.github.com>

=item *

Graham Knop <haarg@haarg.org>

=item *

Karen Etheridge <ether@cpan.org>

=item *

Leon Timmermans <fawaka@gmail.com>

=item *

Paul Howarth <paul@city-fan.org>

=item *

Ricardo Signes <rjbs@semiotic.systems>

=item *

robario <webmaster@robario.com>

=item *

Tatsuhiko Miyagawa <miyagawa@bulknews.net>

=item *

Tatsuhiko Miyagawa <miyagawa@gmail.com>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2010 by David Golden and Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
