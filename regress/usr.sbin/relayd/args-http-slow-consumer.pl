# test that a slow (in this case sleeping) client causes relayd to slow down
# reading from the server (instead of balooning its buffers)

use strict;
use warnings;

my $size = 2**21;

our %args = (
    client => {
	fast => 1,
	max => 100,
	func => sub {
            http_request(@_, $size, "1.0", "");
            http_response(@_, $size);
            print STDERR "going to sleep\n";
            sleep 8;
            read_char(@_, $size);
            return;
        },
        rcvbuf => 2**8,
        nocheck => 1,
    },
    relayd => {
        protocol => [ "http",
            "tcp socket buffer 64",
            "match request header log",
            "match request path log",
        ],
    },
    server => {
        fast => 1,
        func => \&http_server,
        sndbuf => 2**8,
        sndtimeo => 2,
        loggrep => 'short write',

    },
    lengths => [$size],
);

1;
