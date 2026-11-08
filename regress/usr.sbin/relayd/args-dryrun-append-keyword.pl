# test that "append" with a matching keyword is a config error

use strict;
use warnings;

our %args = (
    client => {
	noclient => 1,
    },
    relayd => {
	protocol => [ "http",
	    'match request header append glob "X-*" value "y"',
	],
	dryrun => qr/glob not allowed with append/,
    },
    server => {
	noserver => 1,
    },
    nocheck => 1,
);

1;
