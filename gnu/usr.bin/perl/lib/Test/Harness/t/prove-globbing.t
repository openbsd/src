BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;
use File::Spec;
use Test::More;
plan skip_all => "Not adapted to perl core" if $ENV{PERL_CORE};
plan skip_all => "Not installing prove" if -e "t/SKIP-PROVE";

plan tests => 1;

my $prove = File::Spec->catfile( File::Spec->curdir, "blib", "script", "prove" );
my $tests = File::Spec->catfile( 't', 'prove*.t' );

GLOBBAGE: {
    my @actual = sort qx/$prove --dry $tests/;
    chomp @actual;

    my @expected = (
        File::Spec->catfile( "t", "prove-globbing.t" ),
        File::Spec->catfile( "t", "prove-switches.t" ),
    );
    is_deeply( \@actual, \@expected, "Expands the wildcards" );
}
