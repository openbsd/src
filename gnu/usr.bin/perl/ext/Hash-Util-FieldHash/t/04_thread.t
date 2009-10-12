#!perl

BEGIN {
    if ($ENV{PERL_CORE}) {
	chdir 't' if -d 't';
	@INC = '../lib';
    }
}

use strict; use warnings;
use Test::More;
my $n_tests;

use Hash::Util::FieldHash qw( :all);
my $ob_reg = Hash::Util::FieldHash::_ob_reg;

{
    my $n_basic;
    BEGIN {
        $n_basic = 6; # 6 tests per call of basic_func()
        $n_tests += 5*$n_basic;
    }
    my %h;
    fieldhash %h;

    sub basic_func {
        my $level = shift;
        
        my @res;
        my $push_is = sub {
            my ( $hash, $should, $name) = @_;
            push @res, [ scalar keys %$hash, $should, $name];
        };
            
        my $obj = [];
        $push_is->( \ %h, 0, "$level: initially clear");
        $push_is->( $ob_reg, 0, "$level: ob_reg initially clear");
        $h{ $obj} = 123;
        $push_is->( \ %h, 1, "$level: one object");
        $push_is->( $ob_reg, 1, "$level: ob_reg one object");
        undef $obj;
        $push_is->( \ %h, 0, "$level: garbage collected");
        $push_is->( $ob_reg, 0, "$level: ob_reg garbage collected");
        @res;
    }

    &is( @$_) for basic_func( "home");

    SKIP: {
        require Config;
        skip "No thread support", 3*$n_basic unless
            $Config::Config{ usethreads};
        require threads;
        my ( $t) = threads->create( \ &basic_func, "thread 1");
        &is( @$_) for $t->join;

        &is( @$_) for basic_func( "back home");

        ( $t) = threads->create( sub {
            my ( $t) = threads->create( \ &basic_func, "thread 2");
            $t->join;
        });
        &is( @$_) for $t->join;
    }

    &is( @$_) for basic_func( "back home again");

}

BEGIN { plan tests => $n_tests }
