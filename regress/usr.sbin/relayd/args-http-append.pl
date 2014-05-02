use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	len => 1,
	loggrep => { "X-Server-Append: 127.0.0.1:.*" => 1 },
    },
    relayd => {
	protocol => [ "http",
	    'request header append "$REMOTE_ADDR:$REMOTE_PORT" to X-Client-Append',
	    'response header append "$SERVER_ADDR:$SERVER_PORT" to X-Server-Append',
	],
    },
    server => {
	func => \&http_server,
	loggrep => { "X-Client-Append: 127.0.0.1:.*" => 1 },
    },
);

1;
