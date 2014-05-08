use strict;
use warnings;

my @lengths = (1, 2, 4, 0, 3);
our %args = (
    client => {
	func => sub { eval { http_client(@_) }; warn $@ },
	lengths => \@lengths,
	loggrep => qr/Forbidden/,
    },
    relayd => {
	protocol => [ "http",
	    'return error',
	    'label test_reject_label',
	    'url filter file args-http-filter-url-file.in log',
	    'no label',
	],
	loggrep => {
		qr/rejecting request/ => 1,
		qr/\[test_reject_label\, foo\.bar\/0\:/ => 1
	},
    },
    server => {
	func => \&http_server,
	lengths => (1, 2, 4),
    },
);

1;
