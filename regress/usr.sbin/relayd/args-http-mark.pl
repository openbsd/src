use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "foobar?path",
    },
    relayd => {
	protocol => [ "http",
	    'request path mark "*" from "/foobar" with 55',
	    'request path change "path" to "foobarchangedpath" marked 55',
	],
	loggrep => { ", 55,.*done" => 1 },
    },
    server => {
	func => \&http_server,
    },
);

1;
