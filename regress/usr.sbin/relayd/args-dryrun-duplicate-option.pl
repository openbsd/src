# test that a duplicate lookup type in one rule is a config error

use strict;
use warnings;

our %args = (
    client => {
	noclient => 1,
    },
    relayd => {
	protocol => [ "http",
	    'match request header "X-A" header "X-B"',
	],
	dryrun => qr/duplicate header type in rule/,
    },
    server => {
	noserver => 1,
    },
    nocheck => 1,
);

1;
