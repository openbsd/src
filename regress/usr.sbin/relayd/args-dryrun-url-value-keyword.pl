# test that a glob keyword on the never matched url value is a config error

use strict;
use warnings;

our %args = (
    client => {
	noclient => 1,
    },
    relayd => {
	protocol => [ "http",
	    'match request url "example.com/" value glob "x"',
	],
	dryrun => qr/glob not allowed on a url value, it is never matched/,
    },
    server => {
	noserver => 1,
    },
    nocheck => 1,
);

1;
