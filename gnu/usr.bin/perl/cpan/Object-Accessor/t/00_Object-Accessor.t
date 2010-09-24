 BEGIN { chdir 't' if -d 't' };

use strict;
use lib '../lib';
use Test::More 'no_plan';
use Data::Dumper;

my $Class = 'Object::Accessor';

use_ok($Class);

my $Object  = $Class->new;
my $Acc     = 'foo';
my $Err_re  = qr/No such accessor '$Acc'/;

### stupid warnings
### XXX this will break warning tests though if enabled
$Object::Accessor::DEBUG = $Object::Accessor::DEBUG = 1 if @ARGV;


### check the object
{   ok( $Object,                "Object of '$Class' created" );
    isa_ok( $Object,            $Class );
}

### check non existant accessor
{   my $warning;
    local $SIG{__WARN__} = sub { $warning .= "@_" };

    ok(!$Object->can($Acc),     "Cannot '$Acc'" );
    ok(!$Object->$Acc(),        "   Method '$Acc' returns false" );
    like( $warning, $Err_re,    "   Warning logged" );

    ### check fatal error
    {   local $Object::Accessor::FATAL = 1;
        local $Object::Accessor::FATAL = 1; # stupid warnings

        my $rv = eval { $Object->$Acc() };

        ok( $@,                 "Cannot '$Acc' -- dies" );
        ok(!$rv,                "   Method '$Acc' returns false" );
        like( $@, $Err_re,      "   Fatal error logged" );
    }
}

### create an accessor;
{   my $warning;
    local $SIG{__WARN__} = sub { $warning .= "@_" };

    ok( $Object->mk_accessors( $Acc ),
                                "Accessor '$Acc' created" );

    ok( $Object->can( $Acc ),   "   Can '$Acc'" );
    ok(!$warning,               "   No warnings logged" );
}

### try to use the accessor
{   for my $var ($0, $$) {

        ok( $Object->$Acc( $var ),  "'$Acc' set to '$var'" );
        is( $Object->$Acc(), $var,  "   '$Acc' still holds '$var'" );

        my $sub = $Object->can( $Acc );
        ok( $sub,                   "Retrieved '$Acc' coderef" );
        isa_ok( $sub,               "CODE" );
        is( $sub->(), $var,         "   '$Acc' via coderef holds '$var'" );

        ok( $sub->(1),              "   '$Acc' set via coderef to '1'" );
        is( $Object->$Acc(), 1,     "   '$Acc' still holds '1'" );
    }
}

### get a list of accessors
{   my @list = $Object->ls_accessors;
    ok( scalar(@list),              "Accessors retrieved" );

    for my $acc ( @list ) {
        ok( $Object->can( $acc ),   "   Accessor '$acc' is valid" );
    }

    is_deeply( \@list, [$Acc],      "   Only expected accessors found" );
}

### clone the original
{   my $clone = $Object->mk_clone;
    my @list  = $clone->ls_accessors;

    ok( $clone,                     "Clone created" );
    isa_ok( $clone,                 $Class );
    ok( scalar(@list),              "   Clone has accessors" );
    is_deeply( \@list, [$Object->ls_accessors],
                                    "   Only expected accessors found" );

    for my $acc ( @list ) {
        ok( !defined( $clone->$acc() ),
                                    "   Accessor '$acc' is empty" );
    }
}

### flush the original values
{   my $val = $Object->$Acc();
    ok( $val,                       "Objects '$Acc' has a value" );

    ok( $Object->mk_flush,          "   Object flushed" );
    ok( !$Object->$Acc(),           "   Objects '$Acc' is now empty" );
}

### check that only our original object can do '$Acc'
{   my $warning;
    local $SIG{__WARN__} = sub { $warning .= "@_" };

    my $other = $Class->new;


    ok(!$other->can($Acc),          "Cannot '$Acc' via other object" );
    ok(!$other->$Acc(),             "   Method '$Acc' returns false" );
    like( $warning, $Err_re,        "   Warning logged" );
}

### check if new() passes it's args correctly
{   my $obj = $Class->new( $Acc );
    ok( $obj,                       "Object created with accessors" );
    isa_ok( $obj,                   $Class );
    can_ok( $obj,                   $Acc );
}

1;
