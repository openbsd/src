#! perl

# Verify that loading Getopt::Long does not load Getopt::Long::Parser
# until/unless used.

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;
use warnings;
use Test::More tests => 4;

use_ok("Getopt::Long");

# Getopt::Long::Parser should not be loaded.
ok( !defined $Getopt::Long::Parser::VERSION,
    "Getopt::Long did not load Parser" );

# Create a parser object.
my $p = Getopt::Long::Parser->new;

# Getopt::Long::Parser should now be loaded.
ok( defined $Getopt::Long::Parser::VERSION,
    "Parser $Getopt::Long::Parser::VERSION loaded" );

# Verify version match.
is( $Getopt::Long::VERSION, $Getopt::Long::Parser::VERSION,
    "Parser version matches" );

diag( "Testing Getopt::Long $Getopt::Long::VERSION, Perl $], $^X" );
