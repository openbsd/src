BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir('t') if -d 't';
	@INC = qw(lib/Filter/Simple ../lib);
    }
}

BEGIN { print "1..1\n" }

use ExportTest 'ok';

notok 1;
