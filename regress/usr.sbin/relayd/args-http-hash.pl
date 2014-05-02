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
	    'path hash "/query" log',
	],
	relay => 'forward to <table-$test> port $connectport',
	loggrep => {
		qr/done, \[\/query: foobar\]/ => 1,
		qr/relay_handle_http: hash 0xfde460be/ => 1,
	},
    },
    server => {
	func => \&http_server,
	nocheck => 1,
    },
);

1;
