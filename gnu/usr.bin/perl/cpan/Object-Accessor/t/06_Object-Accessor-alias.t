BEGIN { chdir 't' if -d 't' };

use strict;
use lib '../lib';
use Test::More 'no_plan';
use Data::Dumper;

my $Class = 'Object::Accessor';

use_ok($Class);

my $Object      = $Class->new;
my $Acc         = 'foo';
my $Alias       = 'bar';

### basic sanity test
{   ok( $Object,                "Object created" );
    
    ok( $Object->mk_accessors( $Acc ),
                                "   Accessor ->$Acc created" );
    ok( $Object->$Acc( $$ ),    "   ->$Acc set to $$" );
}

### alias tests
{   ok( $Object->mk_aliases( $Alias => $Acc ),
                                "Alias ->$Alias => ->$Acc" );
    ok( $Object->$Alias,        "   ->$Alias returns value" );
    is( $Object->$Acc, $Object->$Alias,
                                "       ->$Alias eq ->$Acc" );
    ok( $Object->$Alias( $0 ),  "   Set value via alias ->$Alias" );                                  
    is( $Object->$Acc, $Object->$Alias,
                                "       ->$Alias eq ->$Acc" );
}    
