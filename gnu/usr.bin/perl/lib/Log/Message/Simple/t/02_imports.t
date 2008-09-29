use Test::More 'no_plan';
use strict;

my $Class   = 'Log::Message::Simple';
my @Carp    = qw[carp croak cluck confess];
my @Msg     = qw[msg debug error];



### test empty import
{   package Test::A;
    

    eval "use $Class ()";
    Test::More::ok( !$@,        "using $Class with no import" );
    
    for my $func ( @Carp, @Msg ) {
        Test::More::ok( !__PACKAGE__->can( $func ),
                                "   $func not imported" );
    }
}    

### test :STD import
{   package Test::B;

    eval "use $Class ':STD'";
    Test::More::ok( !$@,        "using $Class with :STD  import" );
    
    for my $func ( @Carp ) {
        Test::More::ok( !__PACKAGE__->can( $func ),
                                "   $func not imported" );
    }
    
    for my $func ( @Msg ) {
        Test::More::ok( __PACKAGE__->can( $func ),
                                "   $func imported" );
    }                                
}    

### test :CARP import
{   package Test::C;

    eval "use $Class ':CARP'";
    Test::More::ok( !$@,        "using $Class with :CARP  import" );
    
    for my $func ( @Msg ) {
        Test::More::ok( !__PACKAGE__->can( $func ),
                                "   $func not imported" );
    }
    
    for my $func ( @Carp ) {
        Test::More::ok( __PACKAGE__->can( $func ),
                                "   $func imported" );
    }                                
}    

### test all import

{   package Test::D;

    eval "use $Class ':ALL'";
    Test::More::ok( !$@,        "using $Class with :ALL  import" );
    
    for my $func ( @Carp, @Msg ) {
        Test::More::ok( __PACKAGE__->can( $func ),
                                "   $func imported" );
    }                                
}    
