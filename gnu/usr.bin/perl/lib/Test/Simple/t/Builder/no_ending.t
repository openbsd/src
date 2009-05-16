# $Id: no_ending.t,v 1.1 2009/05/16 21:42:57 simon Exp $
use Test::Builder;

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}

BEGIN {
    my $t = Test::Builder->new;
    $t->no_ending(1);
}

use Test::More tests => 3;

# Normally, Test::More would yell that we ran too few tests, but we
# supressed the ending diagnostics.
pass;
print "ok 2\n";
print "ok 3\n";
