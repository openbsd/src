### Module::Load test suite ###
BEGIN { 
    if( $ENV{PERL_CORE} ) {
        chdir '../lib/Module/Load' if -d '../lib/Module/Load';
        unshift @INC, '../../..';
    }
} 

BEGIN { chdir 't' if -d 't' }

use strict;
use lib qw[../lib to_load];
use Module::Load;
use Test::More tests => 13;


{
    my $mod = 'Must::Be::Loaded';
    my $file = Module::Load::_to_file($mod,1);

    eval { load $mod };

    is( $@, '', qq[Loading module '$mod'] );
    ok( defined($INC{$file}), q[... found in %INC] );
}

{
    my $mod = 'LoadMe.pl';
    my $file = Module::Load::_to_file($mod);

    eval { load $mod };

    is( $@, '', qq[Loading File '$mod'] );
    ok( defined($INC{$file}), q[... found in %INC] );
}

{
    my $mod = 'LoadIt';
    my $file = Module::Load::_to_file($mod,1);

    eval { load $mod };

    is( $@, '', qq[Loading Ambigious Module '$mod'] );
    ok( defined($INC{$file}), q[... found in %INC] );
}

{
    my $mod = 'ToBeLoaded';
    my $file = Module::Load::_to_file($mod);

    eval { load $mod };

    is( $@, '', qq[Loading Ambigious File '$mod'] );
    ok( defined($INC{$file}), q[... found in %INC] );
}

### Test importing functions ###
{   my $mod     = 'TestModule';
    my @funcs   = qw[func1 func2];
    
    eval { load $mod, @funcs };
    is( $@, '', qq[Loaded exporter module '$mod'] );
    
    for my $func (@funcs) {
        ok( $mod->can($func),           "$mod -> can( $func )" );
        ok( __PACKAGE__->can($func),    "we -> can ( $func )"  ); 
    }        
}    
