#!perl -w
use strict;

BEGIN {
 require './test.pl';

 plan (tests => my $tests = 11);

 # Run these at BEGIN time, before arybase loads
 use v5.15;
 is(eval('$[ = 1; 123'), undef);
 like($@, qr/\AAssigning non-zero to \$\[ is no longer possible/);

 if (is_miniperl()) {
   # skip the rest
   SKIP: { skip ("no arybase.xs on miniperl", $tests-2) }
   exit;
 }
}

no warnings 'deprecated';

is(eval('$['), 0);
is(eval('$[ = 0; 123'), 123);
is(eval('$[ = 1; 123'), 123);
$[ = 1;
ok $INC{'arybase.pm'};

use v5.15;
is(eval('$[ = 1; 123'), undef);
like($@, qr/\AAssigning non-zero to \$\[ is no longer possible/);
is $[, 0, '$[ is 0 under 5.16';
$_ = "hello";
/l/g;
my $pos = \pos;
is $$pos, 3;
$$pos = 1;
is $$pos, 1;

1;
