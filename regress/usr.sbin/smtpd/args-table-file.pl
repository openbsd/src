# args-table-file.pl -- file-backed table for virtual address mapping
#
# Tests the file table backend (table_static.c reading from disk) with
# K_VIRTUAL service.  The file is written by Smtpd.pm before smtpd starts.
# Verifies that smtpd correctly loads and queries the file at startup and
# delivers the resolved address via LMTP.

use strict;
use warnings;

our %args = (
    client => {
        func    => \&smtp_client,
        to      => 'filetest@localhost',
        loggrep => qr/250 .* Message accepted for delivery/,
    },
    smtpd => {
        files => {
            'vusers-file.txt' => "filetest\@localhost\tbuild\n",
        },
        config => [
            'table vusers file:$objdir/vusers-file.txt',
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
