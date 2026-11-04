# args-table-db.pl -- Berkeley DB-backed table for virtual address mapping
#
# Tests the db table backend (table_db.c).  Uses makemap(8) to compile a
# text virtual-map file into a .db file before smtpd starts.  Verifies that
# smtpd opens the .db file, performs a K_VIRTUAL lookup, and delivers via LMTP.
#
# Requires: makemap(8) in PATH (standard on OpenBSD; set MAKEMAP env if needed).

use strict;
use warnings;

my $makemap = $ENV{MAKEMAP} || "makemap";

our %args = (
    client => {
        func    => \&smtp_client,
        to      => 'dbtest@localhost',
        loggrep => qr/250 .* Message accepted for delivery/,
    },
    smtpd => {
        # No .txt extension: makemap appends .db to the whole name,
        # so "vusers-db" -> "vusers-db.db" as expected by the config.
        files => {
            'vusers-db' => "dbtest\@localhost\tbuild\n",
        },
	makemap => [ 'vusers-db' ],
        config => [
            'table vusers db:$tempdir/vusers-db.db',
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
