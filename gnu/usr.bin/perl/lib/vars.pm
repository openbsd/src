package vars;

=head1 NAME

vars - Perl pragma to predeclare global variable names

=head1 SYNOPSIS

    use vars qw($frob @mung %seen);

=head1 DESCRIPTION

This will predeclare all the variables whose names are 
in the list, allowing you to use them under "use strict", and
disabling any typo warnings.

See L<perlmod/Pragmatic Modules>.

=cut
require 5.000;
use Carp;

sub import {
    my $callpack = caller;
    my ($pack, @imports, $sym, $ch) = @_;
    foreach $sym (@imports) {
	croak "Can't declare another package's variables" if $sym =~ /::/;
        ($ch, $sym) = unpack('a1a*', $sym);
        *{"${callpack}::$sym"} =
          (  $ch eq "\$" ? \$   {"${callpack}::$sym"}
           : $ch eq "\@" ? \@   {"${callpack}::$sym"}
           : $ch eq "\%" ? \%   {"${callpack}::$sym"}
           : $ch eq "\*" ? \*   {"${callpack}::$sym"}
           : $ch eq "\&" ? \&   {"${callpack}::$sym"}
           : croak "'$ch$sym' is not a valid variable name\n");
    }
};

1;
