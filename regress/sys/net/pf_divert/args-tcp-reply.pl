# test divert-reply
# swap client and server
# server is local
# client diverts packets with reply-to

use strict;
use warnings;

our %args = (
	protocol => "tcp",
	divert => "reply",
);
