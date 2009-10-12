#!/usr/bin/perl -w

BEGIN {
    if ( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ( '../lib', 'lib' );
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;
use lib 't/lib';
use Test::More tests => 1;
use File::Spec;
use App::Prove;

my $prove = App::Prove->new;

$prove->add_rc_file(
    File::Spec->catfile(
        (   $ENV{PERL_CORE}
            ? ( File::Spec->updir(), 'ext', 'Test-Harness' )
            : ()
        ),
        't', 'data',
        'proverc'
    )
);

is_deeply $prove->{rc_opts},
  [ '--should', 'be', '--split', 'correctly', 'Can', 'quote things',
    'using single or', 'double quotes', '--this', 'is', 'OK?'
  ],
  'options parsed';

