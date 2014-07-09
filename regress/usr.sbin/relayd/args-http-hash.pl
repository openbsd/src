use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "query?foobar",
	len => 21,
	nocheck => 1,
    },
    relayd => {
	table => 1,
	protocol => [ "http",
	    'match request path hash "/query"',
	    'match request path log "/query"',
	],
	relay => 'forward to <table-$test> port $connectport',
	loggrep => {
		qr/ (?:done|last write \(done\)), \[\/query: foobar\]/ => 1,
		qr/hashkey 0x7dc0306a/ => 1,
	},
    },
    server => {
	func => \&http_server,
	nocheck => 1,
    },
);

1;
