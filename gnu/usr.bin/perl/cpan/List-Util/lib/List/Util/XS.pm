package List::Util::XS;
use strict;
use vars qw($VERSION);
use List::Util;

$VERSION = "1.23";           # FIXUP
$VERSION = eval $VERSION;    # FIXUP

sub _VERSION { # FIXUP
  require Carp;
  Carp::croak("You need to install Scalar-List-Utils with a C compiler to ensure the XS is compiled")
    if defined $_[1];
  $VERSION;
}

1;
__END__

=head1 NAME

List::Util::XS - Indicate if List::Util was compiled with a C compiler

=head1 SYNOPSIS

    use List::Util::XS 1.20;

=head1 DESCRIPTION

C<List::Util::XS> can be used as a dependency to ensure List::Util was
installed using a C compiler and that the XS version is installed.

During installation C<$List::Util::XS::VERSION> will be set to
C<undef> if the XS was not compiled.

=head1 SEE ALSO

L<Scalar::Util>, L<List::Util>, L<List::MoreUtils>

=head1 COPYRIGHT

Copyright (c) 2008 Graham Barr <gbarr@pobox.com>. All rights reserved.
This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
