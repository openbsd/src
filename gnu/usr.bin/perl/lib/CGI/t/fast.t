#!./perl -w

use lib qw(t/lib);

# Due to a bug in older versions of MakeMaker & Test::Harness, we must
# ensure the blib's are in @INC, else we might use the core CGI.pm
use lib qw(blib/lib blib/arch);

my $fcgi;
BEGIN {
	local $@;
	eval { require FCGI };
	$fcgi = $@ ? 0 : 1;
}

use Test::More tests => 7;

# Shut up "used only once" warnings.
() = $CGI::Q;
() = $CGI::Fast::Ext_Request;

SKIP: {
	skip( 'FCGI not installed, cannot continue', 7 ) unless $fcgi;

	use_ok( CGI::Fast );
	ok( my $q = CGI::Fast->new(), 'created new CGI::Fast object' );
	is( $q, $CGI::Q, 'checking to see if the object was stored properly' );
	is( $q->param(), (), 'no params' );

	ok( $q = CGI::Fast->new({ foo => 'bar' }), 'creating obect with params' );
	is( $q->param('foo'), 'bar', 'checking passed param' );

	# if this is false, the package var will be empty
	$ENV{FCGI_SOCKET_PATH} = 0;
	is( $CGI::Fast::Ext_Request, '', 'checking no active request' );

}
