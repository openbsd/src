#!perl -Tw

BEGIN {
    if ( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;
use Test::More tests => 11;

BEGIN {
    use_ok( 'Test::Harness::Point' );
}

my $point = Test::Harness::Point->new;
isa_ok( $point, 'Test::Harness::Point' );
ok( !$point->ok, "Should start out not OK" );

$point->set_ok( 1 );
ok( $point->ok, "should have turned to true" );

$point->set_ok( 0 );
ok( !$point->ok, "should have turned false" );

$point->set_number( 2112 );
is( $point->number, 2112, "Number is set" );

$point->set_description( "Blah blah" );
is( $point->description, "Blah blah", "Description set" );

$point->set_directive( "Go now" );
is( $point->directive, "Go now", "Directive set" );

$point->add_diagnostic( "# Line 1" );
$point->add_diagnostic( "# Line two" );
$point->add_diagnostic( "# Third line" );
my @diags = $point->diagnostics;
is( @diags, 3, "Three lines" );
is_deeply(
    \@diags,
    [ "# Line 1", "# Line two", "# Third line" ],
    "Diagnostics in list context"
);

my $diagstr = <<EOF;
# Line 1
# Line two
# Third line
EOF

chomp $diagstr;
my $string_diagnostics = $point->diagnostics;
is( $string_diagnostics, $diagstr, "Diagnostics in scalar context" );
