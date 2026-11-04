# args-table-aliases.pl -- file-backed table for local alias expansion (K_ALIAS)
#
# Tests K_ALIAS lookup via the file backend.  smtpd resolves the local part
# of the recipient address ("alias-test") against the aliases table and
# delivers to the mapped user via LMTP.  Exercises the alias expansion path
# distinct from virtual domain mapping.

use strict;
use warnings;

our %args = (
    client => {
        func    => \&smtp_client,
        to      => 'alias-test@localhost',
        loggrep => qr/250 .* Message accepted for delivery/,
    },
    smtpd => {
        files => {
            'aliases-test.txt' => "alias-test\tbuild\n",
        },
        config => [
            'table aliases file:$objdir/aliases-test.txt',
            'action "local" lmtp "$connectaddr:$connectport" alias <aliases>',
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
