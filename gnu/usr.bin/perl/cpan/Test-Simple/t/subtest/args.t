#!/usr/bin/perl -w

use strict;
use Test::Builder;

my $tb = Test::Builder->new;

$tb->ok( !eval { $tb->subtest() } );
$tb->like( $@, qr/^\Qsubtest()'s second argument must be a code ref/ );

$tb->ok( !eval { $tb->subtest("foo") } );
$tb->like( $@, qr/^\Qsubtest()'s second argument must be a code ref/ );

$tb->done_testing();
