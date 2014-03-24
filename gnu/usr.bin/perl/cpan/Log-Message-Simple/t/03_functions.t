use Test::More 'no_plan';
use strict;

my $Class   = 'Log::Message::Simple';
my @Carp    = qw[carp croak cluck confess];
my @Msg     = qw[msg debug error];
my $Text    = 'text';
my $Pkg     = 'Test::A';

use_ok( $Class );

{   package Test::A;

    ### set up local equivalents to exported functions
    ### so we can print to closed FH without having to worry
    ### about warnings
    ### close stderr/warnings for that same purpose, as carp
    ### & friends will print there
    for my $name (@Carp, @Msg) {
        no strict 'refs';
        *$name = sub {
                    local $^W;

                    ### do the block twice to avoid 'used only once'
                    ### warnings
                    local $Log::Message::Simple::ERROR_FH;
                    local $Log::Message::Simple::DEBUG_FH;
                    local $Log::Message::Simple::MSG_FH;

                    local $Log::Message::Simple::ERROR_FH;
                    local $Log::Message::Simple::DEBUG_FH;
                    local $Log::Message::Simple::MSG_FH;




                    local *STDERR;
                    local $SIG{__WARN__} = sub { };

                    my $ref = $Class->can( $name );


                    $ref->( @_ );
                };
    }
}

for my $name (@Carp, @Msg) {

    my $ref = $Pkg->can( $name );
    ok( $ref,                   "Found function for '$name'" );

    ### start with an empty stack?
    cmp_ok( scalar @{[$Class->stack]}, '==', 0,
                                "   Starting with empty stack" );
    ok(!$Class->stack_as_string,"   Stringified stack empty" );

    ### call the func... no output should appear
    ### eval this -- the croak/confess functions die
    eval { $ref->( $Text ); };

    my @stack = $Class->stack;
    cmp_ok( scalar(@stack), '==', 1,
                                "   Text logged to stack" );

    for my $re ( $Text, quotemeta '['.uc($name).']' ) {
        like( $Class->stack_as_string, qr/$re/,
                                "   Text as expected" );
    }

    ### empty stack again ###
    ok( $Class->flush,          "   Stack flushed" );
    cmp_ok( scalar @{[$Class->stack]}, '==', 0,
                                "   Starting with empty stack" );
    ok(!$Class->stack_as_string,"   Stringified stack empty" );
}
