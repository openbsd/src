### make sure we can find our conf.pl file
BEGIN { 
    use FindBin; 
    require "$FindBin::Bin/inc/conf.pl";
}

use strict;
use Test::More 'no_plan';
use CPANPLUS::Error;
use CPANPLUS::Backend;

my $Class       = 'CPANPLUS';
my $ModName     = TEST_CONF_MODULE;
my $Conf        = gimme_conf();
my $CB          = CPANPLUS::Backend->new( $Conf );

### so we get an object with *our* configuration
no warnings 'redefine';
local *CPANPLUS::Backend::new = sub { $CB };

use_ok( $Class );

### install / get / fetch tests
for my $meth ( qw[fetch get install] ) {
    my $sub     = $Class->can( $meth );
    ok( $sub,                   "$Class->can( $meth )" );
    
    my %map = (
        0   => qr/failed/,
        1   => qr/successful/,
    );
    
    ok( 1,                  "Trying '$meth' in different configurations" );
    
    while( my($rv, $re) = each %map ) {
        
        ### don't actually install, just test logic
        no warnings 'redefine';
        local *CPANPLUS::Module::install = sub { $rv };
        local *CPANPLUS::Module::fetch   = sub { $rv };

        CPANPLUS::Error->flush;

        my $ok = $sub->( $ModName );
        is( $ok, $rv,       "   Expected RV: $rv" );
        like( CPANPLUS::Error->stack_as_string, $re,
                            "       With expected diagnostic" );
    }        

    ### does not take objects / references
    {   CPANPLUS::Error->flush;

        my $ok = $sub->( [] );
        ok( !$ok,           "'$meth' with reference does not work" );
        like( CPANPLUS::Error->stack_as_string, qr/object/,
                            "   Error as expected");
    }

    ### requires argument
    {   CPANPLUS::Error->flush;

        my $ok = $sub->( );
        ok( !$ok,           "'$meth' without argument does not work" );
        like( CPANPLUS::Error->stack_as_string, qr/No module specified/,
                            "   Error as expected");
    }
}

### shell tests
{   my $meth = 'shell';
    my $sub  = $Class->can( $meth );

    ok( $sub,               "$Class->can( $meth )" );

    {   ### test package for shell() method
        package CPANPLUS::Shell::Test;
        
        ### ->shell() looks in %INC
        use Module::Loaded qw[mark_as_loaded];
        mark_as_loaded( __PACKAGE__ );

        sub new   { bless {}, __PACKAGE__ };        
        sub shell { $$ };
    }
    
    my $rv = $sub->( 'Test' );
    ok( $rv,                "   Shell started" );
    is( $rv, $$,            "       Proper shell called" );
}

