#!perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}

use Test::More tests => 1;

tie *STDOUT, "Dev::Null" or die $!;

print "not ok 1\n";     # this should not print.
pass 'STDOUT can be mucked with';


package Dev::Null;

sub TIEHANDLE { bless {} }
sub PRINT { 1 }
