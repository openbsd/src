
require 5;
package Pod::Perldoc::BaseTo;
use strict;
use warnings;

sub is_pageable        { '' }
sub write_with_binmode {  1 }

sub output_extension   { 'txt' }  # override in subclass!

# sub new { my $self = shift; ...  }
# sub parse_from_file( my($class, $in, $out) = ...; ... }

#sub new { return bless {}, ref($_[0]) || $_[0] }

sub _perldoc_elem {
  my($self, $name) = splice @_,0,2;
  if(@_) {
    $self->{$name} = $_[0];
  } else {
    $self->{$name};
  }
}


1;

__END__

=head1 NAME

Pod::Perldoc::BaseTo - Base for Pod::Perldoc formatters

=head1 SYNOPSIS

    package Pod::Perldoc::ToMyFormat;

    use base qw( Pod::Perldoc::BaseTo );
    ...

=head1 DESCRIPTION

This package is meant as a base of Pod::Perldoc formatters,
like L<Pod::Perldoc::ToText>, L<Pod::Perldoc::ToMan>, etc.

It provides default implementations for the methods

    is_pageable
    write_with_binmode
    output_extension
    _perldoc_elem

The concrete formatter must implement

    new
    parse_from_file

=head1 SEE ALSO

L<perldoc>

=head1 COPYRIGHT AND DISCLAIMERS

Copyright (c) 2002-2007 Sean M. Burke.

This library is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

This program is distributed in the hope that it will be useful, but
without any warranty; without even the implied warranty of
merchantability or fitness for a particular purpose.

=head1 AUTHOR

Current maintainer: Adriano R. Ferreira <ferreira@cpan.org>

Past contributions from:
Sean M. Burke <sburke@cpan.org>

=cut
