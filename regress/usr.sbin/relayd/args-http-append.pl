use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	len => 1,
	loggrep => {
	    'X-Server-Append: \d+\.\d+\.\d+\.\d+:\d+$' => 1,
	    'Set-Cookie: a=b\;' => 1,
	},
    },
    relayd => {
	protocol => [ "http",
	    'match request header append X-Client-Append value \
		"$REMOTE_ADDR:$REMOTE_PORT"',
	    'match response header append X-Server-Append value \
		"$SERVER_ADDR:$SERVER_PORT" \
		cookie set "a" value "b"',
	],
    },
    server => {
	func => \&http_server,
	loggrep => { 'X-Client-Append: \d+\.\d+\.\d+\.\d+:\d+$' => 1 },
    },
);

1;
