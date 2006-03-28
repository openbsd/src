BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib/';
    }
}
chdir 't';

BEGIN { print "1..1\n" }

use Filter::Simple::ExportTest 'ok';

notok 1;
