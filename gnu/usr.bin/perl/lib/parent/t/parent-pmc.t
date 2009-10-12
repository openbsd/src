#!/usr/bin/perl -w
BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't' if -d 't';
        chdir '../lib/parent';
        @INC = '..';
    }
}

use strict;
use Test::More;
use lib 't/lib';

plan skip_all => ".pmc are only available with 5.6 and later" if $] < 5.006;
plan tests => 3;

use vars qw($got_here);

my $res = eval q{
    package MyTest;

    use parent 'FileThatOnlyExistsAsPMC';

    1
};
my $error = $@;

is $res, 1, "Block ran until the end";
is $error, '', "No error";

my $obj = bless {}, 'FileThatOnlyExistsAsPMC';
can_ok $obj, 'exclaim';
