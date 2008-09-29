### make sure we can find our conf.pl file
BEGIN { 
    use FindBin; 
    require "$FindBin::Bin/inc/conf.pl";
}



BEGIN { chdir 't' if -d 't' };

### this is to make devel::cover happy ###
BEGIN {
    use File::Spec;
    require lib;
    for (qw[../lib inc]) {
        my $l = 'lib'; $l->import(File::Spec->rel2abs($_)) 
    }
}

use strict;
use Test::More 'no_plan';

my $Class = 'CPANPLUS::Dist::Build::Constants';


use_ok( $Class );

for my $name ( qw[BUILD BUILD_DIR] ) {

    my $sub = $Class->can( $name );   
    ok( $sub,                   "$Class can $name" );
    ok( $sub->(),               "   $name called OK" );
}    
