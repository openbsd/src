#!/usr/bin/perl

use strict;
use Test::More tests => 4;

use JSON::PP;

my $json = JSON::PP->new->allow_nonref();

my @vs = $json->incr_parse('"a\"bc');

ok( not scalar(@vs) );

@vs = $json->incr_parse('"');

is( $vs[0], "a\"bc" );


$json = JSON::PP->new;

@vs = $json->incr_parse('"a\"bc');
ok( not scalar(@vs) );
@vs = eval { $json->incr_parse('"') };
ok($@ =~ qr/JSON text must be an object or array/);

