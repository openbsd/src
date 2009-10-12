#!/usr/bin/perl -w

BEGIN {
    if ( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ( '../lib', '../ext/Test-Harness/t/lib' );
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;
use vars qw(%INIT %CUSTOM);

use Test::More tests => 11;
use File::Spec::Functions qw( catfile updir );
use TAP::Parser;

use_ok('MySource');
use_ok('MyPerlSource');
use_ok('MyGrammar');
use_ok('MyIteratorFactory');
use_ok('MyResultFactory');

my @t_path = $ENV{PERL_CORE} ? ( updir(), 'ext', 'Test-Harness' ) : ();
my $source = catfile( @t_path, 't', 'source_tests', 'source' );
my %customize = (
    source_class           => 'MySource',
    perl_source_class      => 'MyPerlSource',
    grammar_class          => 'MyGrammar',
    iterator_factory_class => 'MyIteratorFactory',
    result_factory_class   => 'MyResultFactory',
);
my $p = TAP::Parser->new(
    {   source => $source,
        %customize,
    }
);
ok( $p, 'new customized parser' );

foreach my $key ( keys %customize ) {
    is( $p->$key(), $customize{$key}, "customized $key" );
}

# TODO: make sure these things are propogated down through the parser...
