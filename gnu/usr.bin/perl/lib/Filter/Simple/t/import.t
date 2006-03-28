BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}
chdir 't';

BEGIN { print "1..4\n" }

use lib 'lib';
use Filter::Simple::ImportTest (1..3);

say "not ok 4\n";
