BEGIN { chdir 't' if -d 't' };

use strict;
use lib '../lib';
use Test::More 'no_plan';
use Data::Dumper;

my $Class   = 'Object::Accessor';
my $MyClass = 'My::Class';
my $Acc     = 'foo';

use_ok($Class);

### establish another package that subclasses our own
{   package My::Class;
    use base 'Object::Accessor';
}    

my $Object  = $MyClass->new;

### check the object
{   ok( $Object,                "Object created" );
    isa_ok( $Object,            $MyClass );
    isa_ok( $Object,            $Class );
}    

### create an accessor 
{   ok( $Object->mk_accessors( $Acc ),
                                "Accessor '$Acc' created" );
    ok( $Object->can( $Acc ),   "   Object can '$Acc'" );
    ok( $Object->$Acc(1),       "   Objects '$Acc' set" );
    ok( $Object->$Acc(),        "   Objects '$Acc' retrieved" );
}    
    
### check if we do the right thing when we call an accessor that's
### not a defined function in the base class, and not an accessors 
### in the object either
{   my $sub = eval { $MyClass->can( $$ ); };

    ok( !$sub,                  "No sub from non-existing function" );
    ok( !$@,                    "   Code handled it gracefully" );
}    

### check if a method called on a class, that's not actually there
### doesn't get confused as an object call;
{   eval { $MyClass->$$ };

    ok( $@,                     "Calling '$$' on '$MyClass' dies" );
    like( $@, qr/from somewhere else/,
                                "   Dies with an informative message" );
}                                
