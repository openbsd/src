#!/usr/bin/perl
use strict;
use warnings;
use Test::More tests =>  7;
use ExtUtils::ParseXS;
use ExtUtils::ParseXS::Utilities qw(
  map_type
);

my ($self, $type, $varname);
my ($result, $expected);

$self = ExtUtils::ParseXS->new;

$type = 'struct DATA *';
$varname = 'RETVAL';
$self->{config_RetainCplusplusHierarchicalTypes} = 0;
$expected = "$type\t$varname";
$result = map_type($self, $type, $varname);
is( $result, $expected,
    "Got expected map_type for <$type>, <$varname>, <$self->{config_RetainCplusplusHierarchicalTypes}>" );

$type = 'Crypt::Shark';
$varname = undef;
$self->{config_RetainCplusplusHierarchicalTypes} = 0;
$expected = 'Crypt__Shark';
$result = map_type($self, $type, $varname);
is( $result, $expected,
    "Got expected map_type for <$type>, undef, <$self->{config_RetainCplusplusHierarchicalTypes}>" );

$type = 'Crypt::Shark';
$varname = undef;
$self->{config_RetainCplusplusHierarchicalTypes} = 1;
$expected = 'Crypt::Shark';
$result = map_type($self, $type, $varname);
is( $result, $expected,
    "Got expected map_type for <$type>, undef, <$self->{config_RetainCplusplusHierarchicalTypes}>" );

$type = 'Crypt::TC18';
$varname = 'RETVAL';
$self->{config_RetainCplusplusHierarchicalTypes} = 0;
$expected = "Crypt__TC18\t$varname";
$result = map_type($self, $type, $varname);
is( $result, $expected,
    "Got expected map_type for <$type>, <$varname>, <$self->{config_RetainCplusplusHierarchicalTypes}>" );

$type = 'Crypt::TC18';
$varname = 'RETVAL';
$self->{config_RetainCplusplusHierarchicalTypes} = 1;
$expected = "Crypt::TC18\t$varname";
$result = map_type($self, $type, $varname);
is( $result, $expected,
    "Got expected map_type for <$type>, <$varname>, <$self->{config_RetainCplusplusHierarchicalTypes}>" );

$type = 'array(alpha,beta) gamma';
$varname = 'RETVAL';
$self->{config_RetainCplusplusHierarchicalTypes} = 0;
$expected = "alpha *\t$varname";
$result = map_type($self, $type, $varname);
is( $result, $expected,
    "Got expected map_type for <$type>, <$varname>, <$self->{config_RetainCplusplusHierarchicalTypes}>" );

$type = '(*)';
$varname = 'RETVAL';
$self->{config_RetainCplusplusHierarchicalTypes} = 0;
$expected = "(* $varname )";
$result = map_type($self, $type, $varname);
is( $result, $expected,
    "Got expected map_type for <$type>, <$varname>, <$self->{config_RetainCplusplusHierarchicalTypes}>" );
