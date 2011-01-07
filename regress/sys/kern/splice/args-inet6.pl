# test ipv6 client and server

use strict;
use warnings;

our %args = (
    client => {
	connectdomain => AF_INET6,
	connectaddr => "::1",
    },
    relay => {
	listendomain => AF_INET6,
	listenaddr => "::1",
	connectdomain => AF_INET6,
	connectaddr => "::1",
    },
    server => {
	listendomain => AF_INET6,
	listenaddr => "::1",
    },
);

1;
