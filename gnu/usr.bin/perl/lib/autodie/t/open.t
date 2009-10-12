#!/usr/bin/perl -w
use strict;

use Test::More 'no_plan';

use constant NO_SUCH_FILE => "this_file_had_better_not_exist";

use autodie;

eval { open(my $fh, '<', NO_SUCH_FILE); };
ok($@, "3-arg opening non-existent file fails");
like($@, qr/for reading/, "Well-formatted 3-arg open failure");

eval { open(my $fh, "< ".NO_SUCH_FILE) };
ok($@, "2-arg opening non-existent file fails");

like($@, qr/for reading/, "Well-formatted 2-arg open failure");
unlike($@, qr/GLOB\(0x/, "No ugly globs in 2-arg open messsage");

# RT 47520.  2-argument open without mode would repeat the file
# and line number.

eval {
    use autodie;

    open(my $fh, NO_SUCH_FILE);
};

isa_ok($@, 'autodie::exception');
like(  $@, qr/at \S+ line \d+/, "At least one mention");
unlike($@, qr/at \S+ line \d+\s+at \S+ line \d+/, "...but not too mentions");

# RT 47520-ish.  2-argument open without a mode should be marked
# as 'for reading'.
like($@, qr/for reading/, "Well formatted 2-arg open without mode");

# We also shouldn't get repeated messages, even if the default mode
# was used.  Single-arg open always falls through to the default
# formatter.

eval {
    use autodie;

    open( NO_SUCH_FILE . "" );
};

isa_ok($@, 'autodie::exception');
like(  $@, qr/at \S+ line \d+/, "At least one mention");
unlike($@, qr/at \S+ line \d+\s+at \S+ line \d+/, "...but not too mentions");
