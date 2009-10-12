#!/usr/bin/perl -w
use strict;
use warnings;
use autodie::hints;
use Test::More;

use constant PERL510 => ( $] >= 5.010 );

BEGIN {
    if (not PERL510) {
        plan skip_all => "Only subroutine hints supported in 5.8.x";
    }
    else {
        plan 'no_plan';
    }
}

use FindBin;
use lib "$FindBin::Bin/lib";
use Hints_pod_examples qw(
	undef_scalar false_scalar zero_scalar empty_list default_list
	empty_or_false_list undef_n_error_list foo re_fail bar
	think_positive my_system
);
use autodie qw( !
	undef_scalar false_scalar zero_scalar empty_list default_list
	empty_or_false_list undef_n_error_list foo re_fail bar
	think_positive my_system
);

my %scalar_tests = (

    # Test code             # Exception expected?

    'undef_scalar()'        => 1,
    'undef_scalar(1)',      => 0,
    'undef_scalar(0)',      => 0,
    'undef_scalar("")',     => 0,

    'false_scalar(0)',      => 1,
    'false_scalar()',       => 1,
    'false_scalar(undef)',  => 1,
    'false_scalar("")',     => 1,
    'false_scalar(1)',      => 0,
    'false_scalar("1")',    => 0,

    'zero_scalar("0")',     => 1,
    'zero_scalar(0)',       => 1,
    'zero_scalar(1)',       => 0,
    'zero_scalar(undef)',   => 0,
    'zero_scalar("")',      => 0,

    'foo(0)',	            => 1,
    'foo(undef)',	    => 0,
    'foo(1)',	            => 0,

    'bar(0)',	            => 1,
    'bar(undef)',	    => 0,
    'bar(1)',	            => 0,

    're_fail(-1)',          => 0,
    're_fail("FAIL")',      => 1,
    're_fail("_FAIL")',     => 1,
    're_fail("_fail")',     => 0,
    're_fail("fail")',      => 0,

    'think_positive(-1)'    => 1,
    'think_positive(-2)'    => 1,
    'think_positive(0)'     => 0,
    'think_positive(1)'     => 0,
    'think_positive(2)'     => 0,

    'my_system(1)'          => 1,
    'my_system(2)'          => 1,
    'my_system(0)'          => 0,

);

my %list_tests = (

    'empty_list()',         => 1,
    'empty_list(())',       => 1,
    'empty_list([])',       => 0,
    'empty_list(0)',        => 0,
    'empty_list("")',       => 0,
    'empty_list(undef)',    => 0,

    'default_list()',       => 1,
    'default_list(0)',      => 0,
    'default_list("")',     => 0,
    'default_list(undef)',  => 1,
    'default_list(1)',      => 0,
    'default_list("str")',  => 0,
    'default_list(1, 2)',   => 0,

    'empty_or_false_list()',     => 1,
    'empty_or_false_list(())',   => 1,
    'empty_or_false_list(0)',    => 1,
    'empty_or_false_list(undef)',=> 1,
    'empty_or_false_list("")',   => 1,
    'empty_or_false_list("0")',  => 1,
    'empty_or_false_list(1,2)',  => 0,
    'empty_or_false_list("a")',  => 0,

    'undef_n_error_list(undef, 1)'   => 1,
    'undef_n_error_list(undef, "a")' => 1,
    'undef_n_error_list()'           => 0,
    'undef_n_error_list(0, 1)'       => 0,
    'undef_n_error_list("", 1)'      => 0,
    'undef_n_error_list(1)'          => 0,

    'foo(0)',	            => 1,
    'foo(undef)',	    => 0,
    'foo(1)',	            => 0,

    'bar(0)',	            => 1,
    'bar(undef)',	    => 0,
    'bar(1)',	            => 0,

    're_fail(-1)',          => 1,
    're_fail("FAIL")',      => 0,
    're_fail("_FAIL")',     => 0,
    're_fail("_fail")',     => 0,
    're_fail("fail")',      => 0,

    'think_positive(-1)'    => 1,
    'think_positive(-2)'    => 1,
    'think_positive(0)'     => 0,
    'think_positive(1)'     => 0,
    'think_positive(2)'     => 0,

    'my_system(1)'          => 1,
    'my_system(2)'          => 1,
    'my_system(0)'          => 0,

);

# On Perl 5.8, autodie doesn't correctly propagate into string evals.
# The following snippet forces the use of autodie inside the eval if
# we really really have to.  For 5.10+, we don't want to include this
# fix, because the tests will act as a canary if we screw up string
# eval propagation.

my $perl58_fix = (
    PERL510 ?
    q{} :
    q{use autodie qw(
	undef_scalar false_scalar zero_scalar empty_list default_list
	empty_or_false_list undef_n_error_list foo re_fail bar
	think_positive my_system bizarro_system    
    );}
);

# Some of the tests provide different hints for scalar or list context

while (my ($test, $exception_expected) = each %scalar_tests) {
    eval "
        $perl58_fix
        my \$scalar = $test;
    ";

    if ($exception_expected) {
        isnt("$@", "", "scalar test - $test");
    }
    else {
        is($@, "", "scalar test - $test");
    }
}

while (my ($test, $exception_expected) = each %list_tests) {
    eval "
        $perl58_fix
        my \@array = $test;
    ";

    if ($exception_expected) {
        isnt("$@", "", "array test - $test");
    }
    else {
        is($@, "", "array test - $test");
    }
}

1;
