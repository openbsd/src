package integer;

=head1 NAME

integer - Perl pragma to compute arithmetic in integer instead of double

=head1 SYNOPSIS

    use integer;
    $x = 10/3;
    # $x is now 3, not 3.33333333333333333

=head1 DESCRIPTION

This tells the compiler that it's okay to use integer operations
from here to the end of the enclosing BLOCK.  On many machines, 
this doesn't matter a great deal for most computations, but on those 
without floating point hardware, it can make a big difference.

See L<perlmod/Pragmatic Modules>.

=cut

sub import {
    $^H |= 1;
}

sub unimport {
    $^H &= ~1;
}

1;
