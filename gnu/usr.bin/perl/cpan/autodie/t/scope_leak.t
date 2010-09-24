#!/usr/bin/perl -w
use strict;
use FindBin;

# Check for %^H leaking across file boundries.  Many thanks
# to chocolateboy for pointing out this can be a problem.

use lib $FindBin::Bin;

use Test::More 'no_plan';

use constant NO_SUCH_FILE => 'this_file_had_better_not_exist';
use autodie qw(open);

eval {
    open(my $fh, '<', NO_SUCH_FILE);
};

ok($@, "basic autodie test");

use autodie_test_module;

# If things don't work as they should, then the file we've
# just loaded will still have an autodying main::open (although
# its own open should be unaffected).

eval {
    leak_test(NO_SUCH_FILE);
};

is($@,"","autodying main::open should not leak to other files");

eval {
    autodie_test_module::your_open(NO_SUCH_FILE);
};

is($@,"","Other package open should be unaffected");

# Due to odd filenames reported when doing string evals,
# older versions of autodie would not propogate into string evals.

eval q{
    open(my $fh, '<', NO_SUCH_FILE);
};

TODO: {
    local $TODO = "No known way of propagating into string eval in 5.8"
        if $] < 5.010;

    ok($@, "Failing-open string eval should throw an exception");
    isa_ok($@, 'autodie::exception');
}

eval q{
    no autodie;

    open(my $fh, '<', NO_SUCH_FILE);
};

is("$@","","disabling autodie in string context should work");

eval {
    open(my $fh, '<', NO_SUCH_FILE);
};

ok($@,"...but shouldn't disable it for the calling code.");
isa_ok($@, 'autodie::exception');

eval q{
    no autodie;

    use autodie qw(open);

    open(my $fh, '<', NO_SUCH_FILE);
};

ok($@,"Wacky flipping of autodie in string eval should work too!");
isa_ok($@, 'autodie::exception');
