# test delay before server read, client sends reset during splice

use strict;
use warnings;

our %args = (
    client => {
	func => sub { $SIG{ALRM} = sub { print STDERR "\nShutdown\n"; exit 0 };
	    solingerout(@_); alarm(1); write_char(@_); },
	len => 2**19,
	nocheck => 1,
    },
    relay => {
	func => sub { errignore(@_); relay(@_); },
	rcvbuf => 2**10,
	sndbuf => 2**10,
	down => "Broken pipe|Connection reset by peer",
	nocheck => 1,
    },
    server => {
	func => sub { sleep 3; read_char(@_); },
	nocheck => 1,
    },
    len => 131072,
    md5 => "31e5ad3d0d2aeb1ad8aaa847dfa665c2",
);

1;
