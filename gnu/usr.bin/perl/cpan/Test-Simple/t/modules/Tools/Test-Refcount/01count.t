#!/usr/bin/perl

use strict;
use warnings;

use Test2::API;
use Test2::Tools::Basic;
use Test2::API qw(intercept context);
use Test2::Tools::Compare qw/match subset array event like/;

use Test2::Tools::Refcount;

my $anon = [];

like(
    intercept {
        is_refcount($anon, 1, 'anon ARRAY ref');
    },
    array {
        event Ok => { name => 'anon ARRAY ref', pass => 1 };
    },
    'anon ARRAY ref succeeds'
);

like(
    intercept {
        is_refcount("hello", 1, 'not ref');
    },
    array {
        event Ok => { name => 'not ref', pass => 0 };
        event Diag => { message => match qr/Failed test 'not ref'/ };
        event Diag => { message => "  expected a reference, was not given one" };
    },
    'not ref fails',
);

my $object = bless {}, "Some::Class";

like(
    intercept {
        is_refcount($object, 1, 'object');
    },
    array {
        event Ok => { name => 'object', pass => 1 };
    },
    'normal object succeeds',
);

my $newref = $object;

like(
    intercept {
        is_refcount($object, 2, 'two refs');
    },
    array {
        event Ok => { name => 'two refs', pass => 1 };
    },
    'two refs to object succeeds',
);

like(
    intercept {
        is_refcount($object, 1, 'one ref');
    },
    subset {
        event Ok => { name => 'one ref', pass => 0 };
        event Diag => { message => match qr/Failed test 'one ref'/ };
        event Diag => { message => match qr/expected 1 references, found 2/ };

        if (Test2::Tools::Refcount::HAVE_DEVEL_MAT_DUMPER) {
            event Diag => { message => match qr/SV address is 0x[0-9a-f]+/ };
            event Diag => { message => match qr/Writing heap dump to \S+/ };
        }
    },
    "two refs to object fails to be 1"
);

undef $newref;

$object->{self} = $object;

like(
    intercept {
        is_refcount($object, 2, 'circular');
    },
    array {
        event Ok => { name => 'circular', pass => 1 };
    },
    'circular object succeeds',
);

undef $object->{self};

my $otherobject = bless { firstobject => $object }, "Other::Class";

like(
    intercept {
        is_refcount($object, 2, 'other ref to object');
    },
    array {
        event Ok => { name => 'other ref to object', pass => 1 };
    },
    'object with another reference succeeds',
);

undef $otherobject;

like(
    intercept {
        is_refcount($object, 1, 'undefed other ref to object' );
    },
    array {
        event Ok => { name => 'undefed other ref to object', pass => 1 };
    },
    'object with another reference undefed succeeds',
);

END {
    # Clean up Devel::MAT dumpfile
    my $pmat = $0;
    $pmat =~ s/\.t$/-1.pmat/;
    unlink $pmat if -f $pmat;
}

done_testing;
