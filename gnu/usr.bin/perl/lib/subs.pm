package subs;

=head1 NAME

subs - Perl pragma to predeclare sub names

=head1 SYNOPSIS

    use subs qw(frob);
    frob 3..10;

=head1 DESCRIPTION

This will predeclare all the subroutine whose names are 
in the list, allowing you to use them without parentheses
even before they're declared.

See L<perlmod/Pragmatic Modules> and L<strict/subs>.

=cut
require 5.000;

sub import {
    my $callpack = caller;
    my $pack = shift;
    my @imports = @_;
    foreach $sym (@imports) {
	*{"${callpack}::$sym"} = \&{"${callpack}::$sym"};
    }
};

1;
