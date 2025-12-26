package Scalar::List::Utils;
use strict;
use warnings;

our $VERSION    = "1.69";
$VERSION =~ tr/_//d;

1;

__END__

=head1 NAME

Scalar::List::Utils - A distribution of general-utility subroutines

=head1 SYNOPSIS

    use Scalar::Util qw(blessed);
    use List::Util qw(any);

=head1 DESCRIPTION

C<Scalar::List::Utils> does nothing on its own. It is packaged with several
useful modules.

=head1 MODULES

=head2 L<List::Util>

L<List::Util> contains a selection of useful subroutines for operating on lists
of values.

=head2 L<Scalar::Util>

L<Scalar::Util> contains a selection of useful subroutines for interrogating
or manipulating scalar values.

=head2 L<Sub::Util>

L<Sub::Util> contains a selection of useful subroutines for interrogating
or manipulating subroutine references.

=cut
