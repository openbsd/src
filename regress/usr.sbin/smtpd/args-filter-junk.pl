# args-filter-junk.pl -- filter marks message as junk at mail-from phase
#
# Tests the "junk" filter action.  Unlike reject, junk accepts the message
# but sets the X-Spam flag so smtpd can route it differently.  The message
# still reaches the LMTP sink; we verify both delivery and the junk decision
# in smtpd's log.

use strict;
use warnings;
use Cwd qw(getcwd);

our %args = (
    client => {
        func    => \&smtp_client,
        to      => 'doom@localhost',
        loggrep => qr/250 .* Message accepted for delivery/,
    },
    smtpd => {
        listen_filter  => "junk",
        filter_scripts => [ "filter-stub.pl" ],
        config => [
            'filter "junk" proc-exec "$tempdir/filter-stub.pl mail-from junk"',
            'table vusers { doom@localhost = build }',
            'action "local" lmtp "$connectaddr:$connectport" virtual <vusers>',
            'match from local for local action "local"',
        ],
        loggrep => qr/stat=Delivered/,
    },
    server => {
        func    => \&lmtp_server,
        loggrep => qr/RCPT: RCPT TO:<build>/,
    },
);

1;
