#!./perl

BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
}

use Test::More tests => 8;

# use this first to $CPAN::term can be undefined
use_ok( 'CPAN' );
undef $CPAN::term;

# this kicks off all the magic
use_ok( 'CPAN::Nox' );

# this will be set if $CPAN::term is undefined
is( $CPAN::Suppress_readline, 1, 'should set suppress readline flag' );

# all of these modules have XS components, should be marked unavailable
for my $mod (qw( Digest::MD5 LWP Compress::Zlib )) {
	is( $CPAN::META->has_inst($mod), 0, "$mod should be marked unavailable" );
}

# and these will be set to those in CPAN
is( @CPAN::Nox::EXPORT, @CPAN::EXPORT, 'should export just what CPAN does' );
is( \&CPAN::Nox::AUTOLOAD, \&CPAN::AUTOLOAD, 'AUTOLOAD should be aliased' );
