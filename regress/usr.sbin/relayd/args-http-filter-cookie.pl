# test http block

use strict;
use warnings;

our %args = (
    client => {
	func => sub { eval { http_client(@_) }; warn $@ },
	loggrep => qr/Client missing http 251 response/,
	cookie => "med=thx; domain=.foo.bar; path=/; expires=Mon, 27-Oct-2014 04:11:56 GMT;",
	path => "anypath",
	nocheck => 1,
    },
    relayd => {
	protocol => [ "http",
	    'request cookie filter "thx" from "med" log',
	],
	loggrep => qr/rejecting request, \[med: thx\]/,
    },
    server => {
	noserver => 1,
	nocheck => 1,
    },
);

1;
