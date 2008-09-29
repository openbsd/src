use Test::More tests => 1;

BEGIN {
use_ok( 'Text::Balanced' );
diag( "Testing Text::Balanced $Text::Balanced::VERSION" )
    unless $ENV{PERL_CORE};
}
