# -*-perl-*-
use strict;
use Test qw(:DEFAULT $TESTOUT $TESTERR $ntest);

### This test is crafted in such a way as to prevent Test::Harness from
### seeing the todo tests, otherwise you get people sending in bug reports
### about Test.pm having "UNEXPECTEDLY SUCCEEDED" tests.

open F, ">todo";
$TESTOUT = *F{IO};
$TESTERR = *F{IO};

my $tests = 5; 
plan tests => $tests, todo => [2..$tests]; 

# line 11
ok(1);
ok(1);
ok(0,1);
ok(0,1,"need more tuits");
ok(1,1);

close F;
$TESTOUT = *STDOUT{IO};
$TESTERR = *STDERR{IO};
$ntest = 1;

open F, "todo";
my $out = join '', <F>;
close F;
unlink "todo";

my $expect = <<"EXPECT";
1..5 todo 2 3 4 5;
ok 1
ok 2 # ($0 at line 12 TODO?!)
not ok 3
# Test 3 got: '0' ($0 at line 13 *TODO*)
#   Expected: '1'
not ok 4
# Test 4 got: '0' ($0 at line 14 *TODO*)
#   Expected: '1' (need more tuits)
ok 5 # ($0 at line 15 TODO?!)
EXPECT


print "1..1\n";
ok( $out, $expect );
