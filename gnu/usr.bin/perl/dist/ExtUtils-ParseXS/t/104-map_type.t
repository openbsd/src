#!/usr/bin/perl
use strict;
use warnings;
use Test::More tests =>  7;
use lib qw( lib );
use ExtUtils::ParseXS::Utilities qw(
  map_type
);

my ($self, $type, $varname);
my ($result, $expected);

$type = 'struct DATA *';
$varname = 'RETVAL';
$self->{hiertype} = 0;
$expected = "$type\t$varname";
$result = map_type($self, $type, $varname);
is( $result, $expected,
    "Got expected map_type for <$type>, <$varname>, <$self->{hiertype}>" );

$type = 'Crypt::Shark';
$varname = undef;
$self->{hiertype} = 0;
$expected = 'Crypt__Shark';
$result = map_type($self, $type, $varname);
is( $result, $expected,
    "Got expected map_type for <$type>, undef, <$self->{hiertype}>" );

$type = 'Crypt::Shark';
$varname = undef;
$self->{hiertype} = 1;
$expected = 'Crypt::Shark';
$result = map_type($self, $type, $varname);
is( $result, $expected,
    "Got expected map_type for <$type>, undef, <$self->{hiertype}>" );

$type = 'Crypt::TC18';
$varname = 'RETVAL';
$self->{hiertype} = 0;
$expected = "Crypt__TC18\t$varname";
$result = map_type($self, $type, $varname);
is( $result, $expected,
    "Got expected map_type for <$type>, <$varname>, <$self->{hiertype}>" );

$type = 'Crypt::TC18';
$varname = 'RETVAL';
$self->{hiertype} = 1;
$expected = "Crypt::TC18\t$varname";
$result = map_type($self, $type, $varname);
is( $result, $expected,
    "Got expected map_type for <$type>, <$varname>, <$self->{hiertype}>" );

$type = 'array(alpha,beta) gamma';
$varname = 'RETVAL';
$self->{hiertype} = 0;
$expected = "alpha *\t$varname";
$result = map_type($self, $type, $varname);
is( $result, $expected,
    "Got expected map_type for <$type>, <$varname>, <$self->{hiertype}>" );

$type = '(*)';
$varname = 'RETVAL';
$self->{hiertype} = 0;
$expected = "(* $varname )";
$result = map_type($self, $type, $varname);
is( $result, $expected,
    "Got expected map_type for <$type>, <$varname>, <$self->{hiertype}>" );
