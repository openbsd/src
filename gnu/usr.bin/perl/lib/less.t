#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    push @INC, '../lib';
}

use Test::More tests => 6;

BEGIN { use_ok( 'less' ) }

is_deeply([less->of], [], 'more please');
use less;
is_deeply([less->of], ['please'],'less please');
no less;
is_deeply([less->of],[],'more please');

use less 'random acts';
is_deeply([sort less->of],[sort qw(random acts)],'less random acts');

is(scalar less->of('random'),1,'less random');
