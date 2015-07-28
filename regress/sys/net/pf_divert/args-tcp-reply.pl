# test divert-reply with tcp

use strict;
use warnings;

our %args = (
    protocol => "tcp",
    client => { func => \&write_read_stream },
    server => { func => \&write_read_stream },
    divert => "reply",
);
