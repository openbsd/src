#!/usr/bin/perl

use strict;
use warnings;

use Test2::API;
use Test2::Tools::Basic;
use Test2::API qw(intercept context);
use Test2::Tools::Compare qw/match subset array event like/;

use Scalar::Util qw( weaken );

use Test2::Tools::Refcount;

my $object = bless {}, "Some::Class";

my $newref = $object;

like(
    intercept {
        is_oneref($object, 'one ref');
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

weaken( $newref );

like(
    intercept {
        is_oneref($object, 'object with weakref');
    },
    array {
        event Ok => { name => 'object with weakref', pass => 1 };
    },
    'object with weakref succeeds'
);

END {
    # Clean up Devel::MAT dumpfile
    my $pmat = $0;
    $pmat =~ s/\.t$/-1.pmat/;
    unlink $pmat if -f $pmat;
}

done_testing;
