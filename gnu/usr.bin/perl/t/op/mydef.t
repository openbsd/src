#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;

eval 'my $_';
like $@, qr/^Can't use global \$_ in "my" at /;

done_testing();
