#!perl

BEGIN {
    if ($ENV{PERL_CORE}) {
	chdir 't' if -d 't';
	@INC = '../lib';
    }
}

use Test::More tests => 18;
use Attribute::Handlers;

sub Args : ATTR(CODE) {
    my ($package, $symbol, $referent, $attr, $data, $phase, $filename, $linenum) = @_;
    is( $package,	'main',		'package' );
    is( $symbol,	\*foo,		'symbol' );
    is( $referent,	\&foo,		'referent' );
    is( $attr,		'Args',		'attr' );
    is( ref $data,	'ARRAY',	'data' );
    is( $data->[0],	'bar',		'data' );
    is( $phase,		'CHECK',	'phase' );
    is( $filename,	__FILE__,	'filename' );
    is( $linenum,	26,		'linenum' );
}

sub foo :Args(bar) {}

my $bar :SArgs(grumpf);

sub SArgs : ATTR(SCALAR) {
    my ($package, $symbol, $referent, $attr, $data, $phase, $filename, $linenum) = @_;
    is( $package,	'main',		'package' );
    is( $symbol,	'LEXICAL',	'symbol' );
    is( $referent,	\$bar,		'referent' );
    is( $attr,		'SArgs',	'attr' );
    is( ref $data,	'ARRAY',	'data' );
    is( $data->[0],	'grumpf',	'data' );
    is( $phase,		'CHECK',	'phase' );
    TODO: {
	local $TODO = "Doesn't work correctly";
    is( $filename,	__FILE__,	'filename' );
    is( $linenum,	28,		'linenum' );
    }
}
