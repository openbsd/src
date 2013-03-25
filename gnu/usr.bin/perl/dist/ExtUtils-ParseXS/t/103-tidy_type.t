#!/usr/bin/perl
use strict;
use warnings;
use Test::More tests =>  3;
use lib qw( lib );
use ExtUtils::ParseXS::Utilities qw(
  tidy_type
);

my $input;

$input = ' *  ** ';
is( tidy_type($input), '***',
    "Got expected value for '$input'" );

$input = ' *     ** ';
is( tidy_type($input), '***',
    "Got expected value for '$input'" );

$input = ' *     ** foobar  *  ';
is( tidy_type($input), '*** foobar *',
    "Got expected value for '$input'" );

