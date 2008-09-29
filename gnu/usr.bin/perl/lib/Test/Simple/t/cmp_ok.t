#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;

require Test::Simple::Catch;
my($out, $err) = Test::Simple::Catch::caught();
local $ENV{HARNESS_ACTIVE} = 0;

require Test::Builder;
my $TB = Test::Builder->create;
$TB->level(0);

sub try_cmp_ok {
    my($left, $cmp, $right) = @_;
    
    my %expect;
    $expect{ok}    = eval "\$left $cmp \$right";
    $expect{error} = $@;
    $expect{error} =~ s/ at .*\n?//;

    local $Test::Builder::Level = $Test::Builder::Level + 1;
    my $ok = cmp_ok($left, $cmp, $right);
    $TB->is_num(!!$ok, !!$expect{ok});
    
    my $diag = $err->read;
    if( !$ok and $expect{error} ) {
        $diag =~ s/^# //mg;
        $TB->like( $diag, "/\Q$expect{error}\E/" );
    }
    elsif( $ok ) {
        $TB->is_eq( $diag, '' );
    }
    else {
        $TB->ok(1);
    }
}


use Test::More;
Test::More->builder->no_ending(1);

my @Tests = (
    [1, '==', 1],
    [1, '==', 2],
    ["a", "eq", "b"],
    ["a", "eq", "a"],
    [1, "+", 1],
    [1, "-", 1],
);

# These don't work yet.
if( 0 ) {
#if( eval { require overload } ) {
    require MyOverload;
    
    my $cmp = Overloaded::Compare->new("foo", 42);
    my $ify = Overloaded::Ify->new("bar", 23);
    
    push @Tests, (
        [$cmp, '==', 42],
        [$cmp, 'eq', "foo"],
        [$ify, 'eq', "bar"],
        [$ify, "==", 23],
    );
}

plan tests => scalar @Tests;
$TB->plan(tests => @Tests * 2);

for my $test (@Tests) {
    try_cmp_ok(@$test);
}
