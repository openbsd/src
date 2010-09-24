BEGIN { chdir 't' if -d 't' };

use strict;
use lib '../lib';
use Data::Dumper;

BEGIN {
    require Test::More;
    Test::More->import( 
        # silly bbedit [
        $] >= 5.008         
            ? 'no_plan' 
            : ( skip_all => "Lvalue objects require perl >= 5.8" )
    );
}

my $Class   = 'Object::Accessor';
my $LClass  =  $Class . '::Lvalue';

use_ok($Class);

my $Object      = $LClass->new;
my $Acc         = 'foo';

### stupid warnings
### XXX this will break warning tests though if enabled
$Object::Accessor::DEBUG = $Object::Accessor::DEBUG = 1 if @ARGV;


### check the object
{   ok( $Object,                "Object of '$LClass' created" );
    isa_ok( $Object,            $LClass );
    isa_ok( $Object,            $Class );
    ok( $Object->mk_clone,      "   Object cloned" );
}

### create an accessor;
{   ok( $Object->mk_accessors( $Acc ),
                                "Accessor '$Acc' created" );
    
    eval { $Object->$Acc = $$ };
    ok( !$@,                    "lvalue assign successful $@" );
    ok( $Object->$Acc,          "Accessor '$Acc' set" );
    is( $Object->$Acc, $$,      "   Contains proper value" );
}

### test allow handlers
{   my $acc   = 'bar';
    my $clone = $Object->mk_clone;

    ok( $clone,                 "Cloned the lvalue object" );

    ### lets see if this causes a warning
    {   my $warnings;
        local $SIG{__WARN__} = sub { $warnings .= "@_" };

        ok( $clone->mk_accessors({ $acc => sub { 0 } }),
                                "   Created accessor '$acc'" );
        like( $warnings, qr/not supported/,
                                "       Got warning about allow handlers" );
    }

    ok( eval{ $clone->$acc = $$ },      
                                "   Allow handler ignored" );       
    ok( ! $@,                   "   No error occurred" );
    is( $clone->$acc, $$,       "   Setting '$acc' worked" );
}

### test registering callbacks
{   my $clone = $Object->mk_clone;
    ok( $clone,                 "Cloned the lvalue object" );
    
    {   my $warnings;
        local $SIG{__WARN__} = sub { $warnings .= "@_" };
        ok( ! $clone->register_callback( sub { } ),
                                "Callback not registered" );

        like( $warnings, qr/not supported/,
                                "   Got warning about callbacks" );
    }                                
}

