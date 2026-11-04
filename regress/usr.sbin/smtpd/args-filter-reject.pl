# args-filter-reject.pl -- filter rejects at mail-from phase
#
# Tests the proc-exec filter mechanism.  filter-stub.pl registers for the
# mail-from phase and responds with a 550 reject.  Verifies that smtpd
# propagates the rejection to the SMTP client and logs the disconnect.

use strict;
use warnings;
use Cwd qw(getcwd);

our %args = (
    client => {
        func    => \&smtp_client,
        to      => 'doom@localhost',
        loggrep => qr/MAIL FROM rejected/,
    },
    smtpd => {
        listen_filter  => "reject",
        filter_scripts => [ "filter-stub.pl" ],
        config => [
            'filter "reject" proc-exec "$tempdir/filter-stub.pl mail-from reject"',
            'action "local" lmtp "$connectaddr:$connectport"',
            'match from local for local action "local"',
        ],
        loggrep => qr/smtp disconnected/,
    },
    server => {
        noserver => 1,
    },
);

1;
