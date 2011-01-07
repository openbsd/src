# test ipv6 server

use strict;
use warnings;

our %args = (
    client => {
	connectdomain => AF_INET,
	connectaddr => "127.0.0.1",
    },
    relay => {
	listendomain => AF_INET,
	listenaddr => "127.0.0.1",
	connectdomain => AF_INET6,
	connectaddr => "::1",
    },
    server => {
	listendomain => AF_INET6,
	listenaddr => "::1",
    },
);

1;
