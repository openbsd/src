BEGIN { chdir 't' if -d 't' };

use strict;
use lib '../lib';
use Test::More 'no_plan';
use Data::Dumper;

my $Class = 'Object::Accessor';

use_ok($Class);

my $Object      = $Class->new;
my $Acc         = 'foo';
my $Allow       = qr/^\d+$/;
my $Err_re      = qr/is an invalid value for '$Acc'/;
my ($Ver_re)    = map { qr/$_/ } quotemeta qq['$Acc' (<undef>) is invalid];

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

    ok( $Object->mk_accessors( { $Acc => $Allow } ),
                                "Accessor '$Acc' created" );

    ok( $Object->can( $Acc ),   "   Can '$Acc'" );
    ok(!$warning,               "   No warnings logged" );
    is( $Object->ls_allow( $Acc ), $Allow,
                                "   Proper allow handler stored" );


}

### try to use the accessor
{   ### bad
    {   my $warning;
        local $SIG{__WARN__} = sub { $warning .= "@_" };
    
        ok( !$Object->$Acc( $0 ),   "'$Acc' NOT set to '$0'" );
        is( $Object->$Acc(), undef, "   '$Acc' still holds '<undef>'" );
        like( $warning, $Err_re,    "   Warnings logged" );
    
        ### reset warnings;
        undef $warning;
        
    
        my $ok = $Object->mk_verify;
        ok( !$ok,                   "   Internal verify fails" );
        like( $warning, $Ver_re,    "       Warning logged" );
    }

    $Object->mk_flush;

    ### good
    {   my $warning;
        local $SIG{__WARN__} = sub { $warning .= "@_" };
    
        ok( $Object->$Acc( $$ ),    "'$Acc' set to '$$'" );
        is( $Object->$Acc(), $$,    "   '$Acc' still holds '$$'" );
        ok(!$warning,               "   No warnings logged" );

        ### reset warnings;
        undef $warning;
        
        my $ok = $Object->mk_verify;
        ok( $ok,                    "   Internal verify succeeds" );
        ok( !$warning,              "       No warnings" );

    }

    $Object->mk_flush;

}
