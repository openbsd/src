# test that "log pattern" without a key is a config error, the default
# "*" is no patterns(7) wildcard

use strict;
use warnings;

our %args = (
    client => {
	noclient => 1,
    },
    relayd => {
	protocol => [ "http",
	    'match request header log pattern',
	],
	dryrun => qr/log without a key matches "\*", use glob instead of pattern/,
    },
    server => {
	noserver => 1,
    },
    nocheck => 1,
);

1;
