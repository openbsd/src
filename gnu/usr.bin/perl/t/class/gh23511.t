#!./perl

BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
    set_up_inc('../lib');
}

use v5.36;
use feature 'class';
no warnings 'experimental::class';

# this used to segfault: GH #23511
eval <<'CLASS';
class MyTest {
    $id = 6;
    field $f1 :reader;
    field $f2 :writer;
}
CLASS
like $@, qr/^Global symbol "\$id" requires explicit package name /, "we get the expected 'undeclared variable' error";

done_testing;
