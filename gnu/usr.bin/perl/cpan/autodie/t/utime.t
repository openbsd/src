#!/usr/bin/perl -w
use strict;
use Test::More tests => 4;
use constant NO_SUCH_FILE => "this_file_had_better_not_exist";
use FindBin qw($Bin);
use File::Spec;
use autodie;
use File::Temp qw(tempfile);

my ($fh, $filename) = tempfile;

eval { utime(undef, undef, NO_SUCH_FILE); };
isa_ok($@, 'autodie::exception', 'exception thrown for utime');

my($atime, $mtime) = (stat $filename)[8, 9];

eval { utime(undef, undef, $filename); };
ok(! $@, "We can utime a file just fine.") or diag $@;

eval { utime(undef, undef, NO_SUCH_FILE, $filename); };
isa_ok($@, 'autodie::exception', 'utime exception on single failure.');
is($@->return, 1, "utime fails correctly on a 'true' failure.");
