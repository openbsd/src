BEGIN { chdir 't' if -d 't' };

use strict;
use lib '../lib';
use Test::More 'no_plan';
use Data::Dumper;

my $Class = 'Object::Accessor';

use_ok($Class);

my $Object      = $Class->new;
my $Acc         = 'foo';

### stupid warnings
### XXX this will break warning tests though if enabled
$Object::Accessor::DEBUG = $Object::Accessor::DEBUG = 1 if @ARGV;


### check the object
{   ok( $Object,                "Object of '$Class' created" );
    isa_ok( $Object,            $Class );
}

### create an accessor;
{   my $warning;
    local $SIG{__WARN__} = sub { $warning .= "@_" };

    ok( $Object->mk_accessors( $Acc ),
                                "Accessor '$Acc' created" );

    ok( $Object->can( $Acc ),   "   Can '$Acc'" );
    ok(!$warning,               "   No warnings logged" );


}

### scoped variables
{   ok( 1,                      "Testing scoped values" );

    $Object->$Acc( $$ );
    is( $Object->$Acc, $$,      "   Value set to $$" );
    
    ### set it to a scope
    {   $Object->$Acc( $0 => \my $temp );
        is( $Object->$Acc, $0,  "   Value set to $0" );
    }
    
    is( $Object->$Acc, $$,      "   Value restored to $$" );
}    
