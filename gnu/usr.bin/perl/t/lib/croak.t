#!./perl
# So far, it seems, there is no place to test all the Perl_croak() calls in the
# C code. So this is a start. It's likely that it needs refactoring to be data
# driven. Data driven code exists in various other tests - best plan would be to
# investigate whether any common code library already exists, and if not,
# refactor the "donor" test code into a common code library.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
    plan( tests => 1 );
}

use strict;

fresh_perl_is(<<'EOF', 'No such hook: _HUNGRY at - line 1.', {}, 'Perl_magic_setsig');
$SIG{_HUNGRY} = \&mmm_pie;
warn "Mmm, pie";
EOF
