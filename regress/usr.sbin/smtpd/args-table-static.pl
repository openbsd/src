# args-table-static.pl -- static inline table for virtual address mapping
#
# Tests that smtpd resolves a recipient through a static virtual table
# and delivers the message via LMTP.  This exercises table_static.c and
# the alias/virtual expansion path introduced in Gilles' table engine
# refactor.

use strict;
use warnings;

our %args = (
    client => {
        func    => \&smtp_client,
        to      => 'test@localhost',
        loggrep => qr/250 .* Message accepted for delivery/,
    },
    smtpd => {
        # $connectaddr and $connectport are substituted by Smtpd.pm
        # with the loopback address and port of the LMTP sink.
        # Map directly to a local user to avoid system aliases expansion.
        config  => [
            'table vusers { test@localhost = build }',
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
