#!/usr/bin/perl -w
use strict;
use Test::More;
use FindBin qw($Bin);
use constant TMPFILE => "$Bin/unlink_test_delete_me";

# Create a file to practice unlinking
open(my $fh, ">", TMPFILE)
	or plan skip_all => "Unable to create test file: $!";
print {$fh} "Test\n";
close $fh;

# Check that file now exists
-e TMPFILE or plan skip_all => "Failed to create test file";

# Check we can unlink
unlink TMPFILE;

# Check it's gone
if(-e TMPFILE) {plan skip_all => "Failed to delete test file: $!";}

# Re-create file
open(my $fh2, ">", TMPFILE)
	or plan skip_all => "Unable to create test file: $!";
print {$fh2} "Test\n";
close $fh2;

# Check that file now exists
-e TMPFILE or plan skip_all => "Failed to create test file";

plan tests => 6;

# Try to delete directory (this should succeed)
eval {
	use autodie;

	unlink TMPFILE;
};
is($@, "", "Unlink appears to have been successful");
ok(! -e TMPFILE, "File does not exist");

# Try to delete file again (this should fail)
eval {
	use autodie;

	unlink TMPFILE;
};
ok($@, "Re-unlinking file causes failure.");
isa_ok($@, "autodie::exception", "... errors are of the correct type");
ok($@->matches("unlink"), "... it's also a unlink object");
ok($@->matches(":filesys"), "... and a filesys object");

