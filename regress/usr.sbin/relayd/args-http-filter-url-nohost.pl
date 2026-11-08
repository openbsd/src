
# test that a request without Host header cannot bypass a url allowlist
# relay_httpurl_test() must not treat a missing Host as a url match

use strict;
use warnings;

our %args = (
    client => {
	func => sub {
	    my $self = shift;
	    print <<'EOF';
GET /secret HTTP/1.0

EOF
	    # no http_response($self, 1);
	},
	nocheck => 1,
	method => "GET",
    },
    relayd => {
	protocol => [ "http",
	    'return error',
	    'block',
	    'pass request quick url "foo.bar/public/"',
	],
	loggrep => {
	    qr/Bad Request \(400 Bad Request\)/ => 1,
	    qr/, done/ => 0,
	},
    },
    server => {
	func => \&http_server,
	noserver => 1,
	nocheck => 1,
    },
);

1;
