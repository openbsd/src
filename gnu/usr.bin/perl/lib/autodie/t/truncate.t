#!/usr/bin/perl -w
use strict;

use Test::More;
use File::Temp qw(tempfile);
use IO::Handle;

my $tmpfh = tempfile();
my $truncate_status;

eval {
    $truncate_status = truncate($tmpfh, 0);
};

if ($@ || !defined($truncate_status)) {
    plan skip_all => 'Truncate not implemented or not working on this system';
}

plan tests => 3;

SKIP: {
    my $can_truncate_stdout = truncate(\*STDOUT,0);

    if ($can_truncate_stdout) {
        skip("This system thinks we can truncate STDOUT. Suuure!", 1);
    }

    eval {
        use autodie;
        truncate(\*STDOUT,0);
    };

    isa_ok($@, 'autodie::exception', "Truncating STDOUT should throw an exception");

}

eval {
    use autodie;
    no warnings 'once';
    truncate(\*FOO, 0);
};

isa_ok($@, 'autodie::exception', "Truncating an unopened file is wrong.");

$tmpfh->print("Hello World");
$tmpfh->flush;

eval {
    use autodie;
    truncate($tmpfh, 0);
};

is($@, "", "Truncating a normal file should be fine");
