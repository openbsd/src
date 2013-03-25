BEGIN { chdir 't' if -d 't' };

use strict;
use lib '../lib';
use Test::More 'no_plan';
use Data::Dumper;

my $Class = 'Object::Accessor';

use_ok($Class);

my $Object  = $Class->new;
my $Acc     = 'foo';
my $Alias   = 'bar';

ok( $Object,                "Object created" );
isa_ok( $Object, $Class,    "   Object" );

### add an accessor
{   my $rv = $Object->mk_accessors( $Acc );
    ok( $rv,                "Created accessor '$Acc'" );
    ok( $Object->$Acc( $$ ),"   Set value" );
    is( $Object->$Acc, $$,  "   Retrieved value" );
}

### add an alias
{   my $rv = $Object->mk_aliases( $Alias => $Acc );
    ok( $rv,                "Created alias '$Alias'" );
    ok( $Object->can( $Alias ),
                            "   Alias '$Alias' exists" );
    is( $Object->$Alias, $Object->$Acc,
                            "   Alias & original return the same value" );

    ok( $Object->$Alias( $$.$$ ),
                            "   Changed value using alias" );
    is( $Object->$Alias, $Object->$Acc,
                            "   Alias & original return the same value" );
}

### test if cloning works
{   my $clone = $Object->mk_clone;
    ok( $clone,             "Cloned object" );

    is_deeply( [sort $clone->ls_accessors], [sort $Object->ls_accessors],
                            "   All accessors cloned" );

    ok( $clone->$Acc( $$ ), "   Set value" );
    is( $clone->$Alias, $clone->$Acc,
                            "   Alias & original return the same value" );

    ok( $clone->$Alias( $$.$$ ),
                            "   Changed value using alias" );
    is( $clone->$Alias, $clone->$Acc,
                            "   Alias & original return the same value" );
}

