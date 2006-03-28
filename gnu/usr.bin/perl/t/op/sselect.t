#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = ('.', '../lib');
}

require 'test.pl';

plan (9);

my $blank = "";
eval {select undef, $blank, $blank, 0};
is ($@, "");
eval {select $blank, undef, $blank, 0};
is ($@, "");
eval {select $blank, $blank, undef, 0};
is ($@, "");

eval {select "", $blank, $blank, 0};
is ($@, "");
eval {select $blank, "", $blank, 0};
is ($@, "");
eval {select $blank, $blank, "", 0};
is ($@, "");

eval {select "a", $blank, $blank, 0};
like ($@, qr/^Modification of a read-only value attempted/);
eval {select $blank, "a", $blank, 0};
like ($@, qr/^Modification of a read-only value attempted/);
eval {select $blank, $blank, "a", 0};
like ($@, qr/^Modification of a read-only value attempted/);
