use v5.10;
use strict;
use warnings;
package CPAN::Meta::Requirements::Range;
# ABSTRACT: a set of version requirements for a CPAN dist

our $VERSION = '2.143';

use Carp ();

#pod =head1 SYNOPSIS
#pod
#pod   use CPAN::Meta::Requirements::Range;
#pod
#pod   my $range = CPAN::Meta::Requirements::Range->with_minimum(1);
#pod
#pod   $range = $range->with_maximum('v2.2');
#pod
#pod   my $stringified = $range->as_string;
#pod
#pod =head1 DESCRIPTION
#pod
#pod A CPAN::Meta::Requirements::Range object models a set of version constraints like
#pod those specified in the F<META.yml> or F<META.json> files in CPAN distributions,
#pod and as defined by L<CPAN::Meta::Spec>;
#pod It can be built up by adding more and more constraints, and it will reduce them
#pod to the simplest representation.
#pod
#pod Logically impossible constraints will be identified immediately by thrown
#pod exceptions.
#pod
#pod =cut

use Carp ();

package
  CPAN::Meta::Requirements::Range::_Base;

# To help ExtUtils::MakeMaker bootstrap CPAN::Meta::Requirements on perls
# before 5.10, we fall back to the EUMM bundled compatibility version module if
# that's the only thing available.  This shouldn't ever happen in a normal CPAN
# install of CPAN::Meta::Requirements, as version.pm will be picked up from
# prereqs and be available at runtime.

BEGIN {
  eval "use version ()"; ## no critic
  if ( my $err = $@ ) {
    eval "use ExtUtils::MakeMaker::version" or die $err; ## no critic
  }
}

# from version::vpp
sub _find_magic_vstring {
  my $value = shift;
  my $tvalue = '';
  require B;
  my $sv = B::svref_2object(\$value);
  my $magic = ref($sv) eq 'B::PVMG' ? $sv->MAGIC : undef;
  while ( $magic ) {
    if ( $magic->TYPE eq 'V' ) {
      $tvalue = $magic->PTR;
      $tvalue =~ s/^v?(.+)$/v$1/;
      last;
    }
    else {
      $magic = $magic->MOREMAGIC;
    }
  }
  return $tvalue;
}

# Perl 5.10.0 didn't have "is_qv" in version.pm
*_is_qv = version->can('is_qv') ? sub { $_[0]->is_qv } : sub { exists $_[0]->{qv} };

# construct once, reuse many times
my $V0 = version->new(0);

# safe if given an unblessed reference
sub _isa_version {
  UNIVERSAL::isa( $_[0], 'UNIVERSAL' ) && $_[0]->isa('version')
}

sub _version_object {
  my ($self, $version, $module, $bad_version_hook) = @_;

  my ($vobj, $err);

  if (not defined $version or (!ref($version) && $version eq '0')) {
    return $V0;
  }
  elsif ( ref($version) eq 'version' || ( ref($version) && _isa_version($version) ) ) {
    $vobj = $version;
  }
  else {
    # hack around version::vpp not handling <3 character vstring literals
    if ( $INC{'version/vpp.pm'} || $INC{'ExtUtils/MakeMaker/version/vpp.pm'} ) {
      my $magic = _find_magic_vstring( $version );
      $version = $magic if length $magic;
    }
    # pad to 3 characters if before 5.8.1 and appears to be a v-string
    if ( $] < 5.008001 && $version !~ /\A[0-9]/ && substr($version,0,1) ne 'v' && length($version) < 3 ) {
      $version .= "\0" x (3 - length($version));
    }
    eval {
      local $SIG{__WARN__} = sub { die "Invalid version: $_[0]" };
      # avoid specific segfault on some older version.pm versions
      die "Invalid version: $version" if $version eq 'version';
      $vobj = version->new($version);
    };
    if ( my $err = $@ ) {
      $vobj = eval { $bad_version_hook->($version, $module) }
        if ref $bad_version_hook eq 'CODE';
      unless (eval { $vobj->isa("version") }) {
        $err =~ s{ at .* line \d+.*$}{};
        die "Can't convert '$version': $err";
      }
    }
  }

  # ensure no leading '.'
  if ( $vobj =~ m{\A\.} ) {
    $vobj = version->new("0$vobj");
  }

  # ensure normal v-string form
  if ( _is_qv($vobj) ) {
    $vobj = version->new($vobj->normal);
  }

  return $vobj;
}

#pod =method with_string_requirement
#pod
#pod   $req->with_string_requirement('>= 1.208, <= 2.206');
#pod   $req->with_string_requirement(v1.208);
#pod
#pod This method parses the passed in string and adds the appropriate requirement.
#pod A version can be a Perl "v-string".  It understands version ranges as described
#pod in the L<CPAN::Meta::Spec/Version Ranges>. For example:
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

my %methods_for_op = (
  '==' => [ qw(with_exact_version) ],
  '!=' => [ qw(with_exclusion) ],
  '>=' => [ qw(with_minimum)   ],
  '<=' => [ qw(with_maximum)   ],
  '>'  => [ qw(with_minimum with_exclusion) ],
  '<'  => [ qw(with_maximum with_exclusion) ],
);

sub with_string_requirement {
  my ($self, $req, $module, $bad_version_hook) = @_;
  $module //= 'module';

  unless ( defined $req && length $req ) {
    $req = 0;
    Carp::carp("Undefined requirement for $module treated as '0'");
  }

  my $magic = _find_magic_vstring( $req );
  if (length $magic) {
    return $self->with_minimum($magic, $module, $bad_version_hook);
  }

  my @parts = split qr{\s*,\s*}, $req;

  for my $part (@parts) {
    my ($op, $ver) = $part =~ m{\A\s*(==|>=|>|<=|<|!=)\s*(.*)\z};

    if (! defined $op) {
      $self = $self->with_minimum($part, $module, $bad_version_hook);
    } else {
      Carp::croak("illegal requirement string: $req")
        unless my $methods = $methods_for_op{ $op };

      $self = $self->$_($ver, $module, $bad_version_hook) for @$methods;
    }
  }

  return $self;
}

#pod =method with_range
#pod
#pod  $range->with_range($other_range)
#pod
#pod This creates a new range object that is a merge two others.
#pod
#pod =cut

sub with_range {
  my ($self, $other, $module, $bad_version_hook) = @_;
  for my $modifier($other->_as_modifiers) {
    my ($method, $arg) = @$modifier;
    $self = $self->$method($arg, $module, $bad_version_hook);
  }
  return $self;
}

package CPAN::Meta::Requirements::Range;

our @ISA = 'CPAN::Meta::Requirements::Range::_Base';

sub _clone {
  return (bless { } => $_[0]) unless ref $_[0];

  my ($s) = @_;
  my %guts = (
    (exists $s->{minimum} ? (minimum => version->new($s->{minimum})) : ()),
    (exists $s->{maximum} ? (maximum => version->new($s->{maximum})) : ()),

    (exists $s->{exclusions}
      ? (exclusions => [ map { version->new($_) } @{ $s->{exclusions} } ])
      : ()),
  );

  bless \%guts => ref($s);
}

#pod =method with_exact_version
#pod
#pod   $range->with_exact_version( $version );
#pod
#pod This sets the version required to I<exactly> the given
#pod version.  No other version would be considered acceptable.
#pod
#pod This method returns the version range object.
#pod
#pod =cut

sub with_exact_version {
  my ($self, $version, $module, $bad_version_hook) = @_;
  $module //= 'module';
  $self = $self->_clone;
  $version = $self->_version_object($version, $module, $bad_version_hook);

  unless ($self->accepts($version)) {
    $self->_reject_requirements(
      $module,
      "exact specification $version outside of range " . $self->as_string
    );
  }

  return CPAN::Meta::Requirements::Range::_Exact->_new($version);
}

sub _simplify {
  my ($self, $module) = @_;

  if (defined $self->{minimum} and defined $self->{maximum}) {
    if ($self->{minimum} == $self->{maximum}) {
      if (grep { $_ == $self->{minimum} } @{ $self->{exclusions} || [] }) {
        $self->_reject_requirements(
          $module,
          "minimum and maximum are both $self->{minimum}, which is excluded",
        );
      }

      return CPAN::Meta::Requirements::Range::_Exact->_new($self->{minimum});
    }

    if ($self->{minimum} > $self->{maximum}) {
      $self->_reject_requirements(
        $module,
        "minimum $self->{minimum} exceeds maximum $self->{maximum}",
      );
    }
  }

  # eliminate irrelevant exclusions
  if ($self->{exclusions}) {
    my %seen;
    @{ $self->{exclusions} } = grep {
      (! defined $self->{minimum} or $_ >= $self->{minimum})
      and
      (! defined $self->{maximum} or $_ <= $self->{maximum})
      and
      ! $seen{$_}++
    } @{ $self->{exclusions} };
  }

  return $self;
}

#pod =method with_minimum
#pod
#pod   $range->with_minimum( $version );
#pod
#pod This adds a new minimum version requirement.  If the new requirement is
#pod redundant to the existing specification, this has no effect.
#pod
#pod Minimum requirements are inclusive.  C<$version> is required, along with any
#pod greater version number.
#pod
#pod This method returns the version range object.
#pod
#pod =cut

sub with_minimum {
  my ($self, $minimum, $module, $bad_version_hook) = @_;
  $module //= 'module';
  $self = $self->_clone;
  $minimum = $self->_version_object( $minimum, $module, $bad_version_hook );

  if (defined (my $old_min = $self->{minimum})) {
    $self->{minimum} = (sort { $b cmp $a } ($minimum, $old_min))[0];
  } else {
    $self->{minimum} = $minimum;
  }

  return $self->_simplify($module);
}

#pod =method with_maximum
#pod
#pod   $range->with_maximum( $version );
#pod
#pod This adds a new maximum version requirement.  If the new requirement is
#pod redundant to the existing specification, this has no effect.
#pod
#pod Maximum requirements are inclusive.  No version strictly greater than the given
#pod version is allowed.
#pod
#pod This method returns the version range object.
#pod
#pod =cut

sub with_maximum {
  my ($self, $maximum, $module, $bad_version_hook) = @_;
  $module //= 'module';
  $self = $self->_clone;
  $maximum = $self->_version_object( $maximum, $module, $bad_version_hook );

  if (defined (my $old_max = $self->{maximum})) {
    $self->{maximum} = (sort { $a cmp $b } ($maximum, $old_max))[0];
  } else {
    $self->{maximum} = $maximum;
  }

  return $self->_simplify($module);
}

#pod =method with_exclusion
#pod
#pod   $range->with_exclusion( $version );
#pod
#pod This adds a new excluded version.  For example, you might use these three
#pod method calls:
#pod
#pod   $range->with_minimum( '1.00' );
#pod   $range->with_maximum( '1.82' );
#pod
#pod   $range->with_exclusion( '1.75' );
#pod
#pod Any version between 1.00 and 1.82 inclusive would be acceptable, except for
#pod 1.75.
#pod
#pod This method returns the requirements object.
#pod
#pod =cut

sub with_exclusion {
  my ($self, $exclusion, $module, $bad_version_hook) = @_;
  $module //= 'module';
  $self = $self->_clone;
  $exclusion = $self->_version_object( $exclusion, $module, $bad_version_hook );

  push @{ $self->{exclusions} ||= [] }, $exclusion;

  return $self->_simplify($module);
}

sub _as_modifiers {
  my ($self) = @_;
  my @mods;
  push @mods, [ with_minimum => $self->{minimum} ] if exists $self->{minimum};
  push @mods, [ with_maximum => $self->{maximum} ] if exists $self->{maximum};
  push @mods, map {; [ with_exclusion => $_ ] } @{$self->{exclusions} || []};
  return @mods;
}

#pod =method as_struct
#pod
#pod   $range->as_struct( $module );
#pod
#pod This returns a data structure containing the version requirements. This should
#pod not be used for version checks (see L</accepts_module> instead).
#pod
#pod =cut

sub as_struct {
  my ($self) = @_;

  return 0 if ! keys %$self;

  my @exclusions = @{ $self->{exclusions} || [] };

  my @parts;

  for my $tuple (
    [ qw( >= > minimum ) ],
    [ qw( <= < maximum ) ],
  ) {
    my ($op, $e_op, $k) = @$tuple;
    if (exists $self->{$k}) {
      my @new_exclusions = grep { $_ != $self->{ $k } } @exclusions;
      if (@new_exclusions == @exclusions) {
        push @parts, [ $op, "$self->{ $k }" ];
      } else {
        push @parts, [ $e_op, "$self->{ $k }" ];
        @exclusions = @new_exclusions;
      }
    }
  }

  push @parts, map {; [ "!=", "$_" ] } @exclusions;

  return \@parts;
}

#pod =method as_string
#pod
#pod   $range->as_string;
#pod
#pod This returns a string containing the version requirements in the format
#pod described in L<CPAN::Meta::Spec>. This should only be used for informational
#pod purposes such as error messages and should not be interpreted or used for
#pod comparison (see L</accepts> instead).
#pod
#pod =cut

sub as_string {
  my ($self) = @_;

  my @parts = @{ $self->as_struct };

  return $parts[0][1] if @parts == 1 and $parts[0][0] eq '>=';

  return join q{, }, map {; join q{ }, @$_ } @parts;
}

sub _reject_requirements {
  my ($self, $module, $error) = @_;
  Carp::croak("illegal requirements for $module: $error")
}

#pod =method accepts
#pod
#pod   my $bool = $range->accepts($version);
#pod
#pod Given a version, this method returns true if the version specification
#pod accepts the provided version.  In other words, given:
#pod
#pod   '>= 1.00, < 2.00'
#pod
#pod We will accept 1.00 and 1.75 but not 0.50 or 2.00.
#pod
#pod =cut

sub accepts {
  my ($self, $version) = @_;

  return if defined $self->{minimum} and $version < $self->{minimum};
  return if defined $self->{maximum} and $version > $self->{maximum};
  return if defined $self->{exclusions}
        and grep { $version == $_ } @{ $self->{exclusions} };

  return 1;
}

#pod =method is_simple
#pod
#pod This method returns true if and only if the range is an inclusive minimum
#pod -- that is, if their string expression is just the version number.
#pod
#pod =cut

sub is_simple {
  my ($self) = @_;
  # XXX: This is a complete hack, but also entirely correct.
  return if $self->as_string =~ /\s/;

  return 1;
}

package
  CPAN::Meta::Requirements::Range::_Exact;

our @ISA = 'CPAN::Meta::Requirements::Range::_Base';

our $VERSION = '2.141';

BEGIN {
  eval "use version ()"; ## no critic
  if ( my $err = $@ ) {
    eval "use ExtUtils::MakeMaker::version" or die $err; ## no critic
  }
}

sub _new      { bless { version => $_[1] } => $_[0] }

sub accepts { return $_[0]{version} == $_[1] }

sub _reject_requirements {
  my ($self, $module, $error) = @_;
  Carp::croak("illegal requirements for $module: $error")
}

sub _clone {
  (ref $_[0])->_new( version->new( $_[0]{version} ) )
}

sub with_exact_version {
  my ($self, $version, $module, $bad_version_hook) = @_;
  $module //= 'module';
  $version = $self->_version_object($version, $module, $bad_version_hook);

  return $self->_clone if $self->accepts($version);

  $self->_reject_requirements(
    $module,
    "can't be exactly $version when exact requirement is already $self->{version}",
  );
}

sub with_minimum {
  my ($self, $minimum, $module, $bad_version_hook) = @_;
  $module //= 'module';
  $minimum = $self->_version_object( $minimum, $module, $bad_version_hook );

  return $self->_clone if $self->{version} >= $minimum;
  $self->_reject_requirements(
    $module,
    "minimum $minimum exceeds exact specification $self->{version}",
  );
}

sub with_maximum {
  my ($self, $maximum, $module, $bad_version_hook) = @_;
  $module //= 'module';
  $maximum = $self->_version_object( $maximum, $module, $bad_version_hook );

  return $self->_clone if $self->{version} <= $maximum;
  $self->_reject_requirements(
    $module,
    "maximum $maximum below exact specification $self->{version}",
  );
}

sub with_exclusion {
  my ($self, $exclusion, $module, $bad_version_hook) = @_;
  $module //= 'module';
  $exclusion = $self->_version_object( $exclusion, $module, $bad_version_hook );

  return $self->_clone unless $exclusion == $self->{version};
  $self->_reject_requirements(
    $module,
    "tried to exclude $exclusion, which is already exactly specified",
  );
}

sub as_string { return "== $_[0]{version}" }

sub as_struct { return [ [ '==', "$_[0]{version}" ] ] }

sub _as_modifiers { return [ with_exact_version => $_[0]{version} ] }


1;

# vim: ts=2 sts=2 sw=2 et:

__END__

=pod

=encoding UTF-8

=head1 NAME

CPAN::Meta::Requirements::Range - a set of version requirements for a CPAN dist

=head1 VERSION

version 2.143

=head1 SYNOPSIS

  use CPAN::Meta::Requirements::Range;

  my $range = CPAN::Meta::Requirements::Range->with_minimum(1);

  $range = $range->with_maximum('v2.2');

  my $stringified = $range->as_string;

=head1 DESCRIPTION

A CPAN::Meta::Requirements::Range object models a set of version constraints like
those specified in the F<META.yml> or F<META.json> files in CPAN distributions,
and as defined by L<CPAN::Meta::Spec>;
It can be built up by adding more and more constraints, and it will reduce them
to the simplest representation.

Logically impossible constraints will be identified immediately by thrown
exceptions.

=head1 METHODS

=head2 with_string_requirement

  $req->with_string_requirement('>= 1.208, <= 2.206');
  $req->with_string_requirement(v1.208);

This method parses the passed in string and adds the appropriate requirement.
A version can be a Perl "v-string".  It understands version ranges as described
in the L<CPAN::Meta::Spec/Version Ranges>. For example:

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

=head2 with_range

 $range->with_range($other_range)

This creates a new range object that is a merge two others.

=head2 with_exact_version

  $range->with_exact_version( $version );

This sets the version required to I<exactly> the given
version.  No other version would be considered acceptable.

This method returns the version range object.

=head2 with_minimum

  $range->with_minimum( $version );

This adds a new minimum version requirement.  If the new requirement is
redundant to the existing specification, this has no effect.

Minimum requirements are inclusive.  C<$version> is required, along with any
greater version number.

This method returns the version range object.

=head2 with_maximum

  $range->with_maximum( $version );

This adds a new maximum version requirement.  If the new requirement is
redundant to the existing specification, this has no effect.

Maximum requirements are inclusive.  No version strictly greater than the given
version is allowed.

This method returns the version range object.

=head2 with_exclusion

  $range->with_exclusion( $version );

This adds a new excluded version.  For example, you might use these three
method calls:

  $range->with_minimum( '1.00' );
  $range->with_maximum( '1.82' );

  $range->with_exclusion( '1.75' );

Any version between 1.00 and 1.82 inclusive would be acceptable, except for
1.75.

This method returns the requirements object.

=head2 as_struct

  $range->as_struct( $module );

This returns a data structure containing the version requirements. This should
not be used for version checks (see L</accepts_module> instead).

=head2 as_string

  $range->as_string;

This returns a string containing the version requirements in the format
described in L<CPAN::Meta::Spec>. This should only be used for informational
purposes such as error messages and should not be interpreted or used for
comparison (see L</accepts> instead).

=head2 accepts

  my $bool = $range->accepts($version);

Given a version, this method returns true if the version specification
accepts the provided version.  In other words, given:

  '>= 1.00, < 2.00'

We will accept 1.00 and 1.75 but not 0.50 or 2.00.

=head2 is_simple

This method returns true if and only if the range is an inclusive minimum
-- that is, if their string expression is just the version number.

=head1 AUTHORS

=over 4

=item *

David Golden <dagolden@cpan.org>

=item *

Ricardo Signes <rjbs@cpan.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2010 by David Golden and Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
