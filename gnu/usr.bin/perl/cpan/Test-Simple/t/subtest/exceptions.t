#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ( '../lib', 'lib' );
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;
use warnings;
use Test::Builder::NoOutput;
use Test::More tests => 7;

{
    my $tb = Test::Builder::NoOutput->create;
    $tb->child('one');
    eval { $tb->child('two') };
    my $error = $@;
    like $error, qr/\QYou already have a child named (one) running/,
      'Trying to create a child with another one active should fail';
}
{
    my $tb    = Test::Builder::NoOutput->create;
    my $child = $tb->child('one');
    ok my $child2 = $child->child('two'), 'Trying to create nested children should succeed';
    eval { $child->finalize };
    my $error = $@;
    like $error, qr/\QCan't call finalize() with child (two) active/,
      '... but trying to finalize() a child with open children should fail';
}
{
    my $tb    = Test::Builder::NoOutput->create;
    my $child = $tb->child('one');
    undef $child;
    like $tb->read, qr/\QChild (one) exited without calling finalize()/,
      'Failing to call finalize should issue an appropriate diagnostic';
    ok !$tb->is_passing, '... and should cause the test suite to fail';
}
{
    my $tb = Test::Builder::NoOutput->create;

    $tb->plan( tests => 7 );
    for( 1 .. 3 ) {
        $tb->ok( $_, "We're on $_" );
        $tb->diag("We ran $_");
    }
    {
        my $indented = $tb->child;
        $indented->plan('no_plan');
        $indented->ok( 1, "We're on 1" );
        eval { $tb->ok( 1, 'This should throw an exception' ) };
        $indented->finalize;
    }

    my $error = $@;
    like $error, qr/\QCannot run test (This should throw an exception) with active children/,
      'Running a test with active children should fail';
    ok !$tb->is_passing, '... and should cause the test suite to fail';
}
