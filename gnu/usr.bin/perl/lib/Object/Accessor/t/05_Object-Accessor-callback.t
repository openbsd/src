BEGIN { chdir 't' if -d 't' };

use strict;
use lib '../lib';
use Test::More 'no_plan';
use Data::Dumper;

my $Class   = 'Object::Accessor';
my $LClass  = $Class . '::Lvalue';

use_ok($Class);

### stupid warnings
### XXX this will break warning tests though if enabled
$Object::Accessor::DEBUG = $Object::Accessor::DEBUG = 1 if @ARGV;

my $Object      = $Class->new;
my $Acc         = 'foo';
my $Func        = 'register_callback';
my $Called      = 0;
my $RetVal      = $$;
my $SetVal      = 1;

### 6 tests
my $Sub         = sub {
        my $obj     = shift;
        my $meth    = shift;
        my $val     = shift;
    
        $Called++;
        
        ok( 1,                  "   In callback now" );
        ok( $obj,               "       Object received" );
        isa_ok( $obj, $Class,   "       Object");
        is( $meth, $Acc,        "       Method is '$Acc'" );
        isa_ok( $val, "ARRAY",  "       Value" );
        scalar @$val 
            ? is( $val->[0], $SetVal,
                                "       Attempted to set $SetVal" )
            : ok( ! exists $val->[0],
                                "       This was a GET request" );

        return $RetVal;
};

### set up the object
{   ok( $Object,                "Object created" );
    isa_ok( $Object,            $Class );
    ok( $Object->mk_accessors( $Acc ),
                                "   Accessor '$Acc' created" );
    can_ok( $Object,            $Func );
    ok( $Object->$Func( $Sub ), "   Callback registered" );
}

### test ___get and ___set
{   $Called = 0;

    my $clone = $Object->mk_clone;
    ok( $clone,                 "Object cloned" );
    
    my $val = $clone->___get($Acc);
    is( $val, undef,            "   Direct get returns <undef>" );
    ok( $clone->___set( $Acc => $SetVal ),
                                "   Direct set is able to set the value" );
    is( $clone->___get( $Acc ), $SetVal,
                                "   Direct get returns $SetVal" );
    ok( !$Called,               "   Callbacks didn't get called" );                                
}

### test callbacks on regular objects
### XXX callbacks DO NOT work on lvalue objects. This is verified
### in the lvalue test file, so we dont test here
{   #diag("Running GET tests on regular objects");
    
    my $clone   = $Object->mk_clone;

    $Called = 0;
    is( $clone->$Acc, $RetVal,   "   Method '$Acc' returns '$RetVal' " );
    is( $clone->___get($Acc), undef,
                                "   Direct get returns <undef>" );    
    ok( $Called,                "   Callback called" );

    
    #diag("Running SET tests on regular objects");
    $Called = 0;
    ok( $clone->$Acc($SetVal),  "   Setting $Acc" );
    ok( $Called,                "   Callback called" );

    $Called = 0;
    is( $clone->$Acc, $RetVal,  "   Returns $RetVal" );
    ok( $Called,                "   Callback called" );

    $Called = 0;
    is( $clone->___get( $Acc ), $RetVal,
                                "   Direct get returns $RetVal" );
    ok( !$Called,               "   Callback not called" );
}
