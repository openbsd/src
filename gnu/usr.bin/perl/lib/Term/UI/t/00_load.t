use Test::More 'no_plan';
use strict;

BEGIN { 
    chdir 't' if -d 't';
    use File::Spec;
    use lib File::Spec->catdir( qw[.. lib] );
}

my $Class = 'Term::UI';

use_ok( $Class );

diag "Testing $Class " . $Class->VERSION unless $ENV{PERL_CORE};
