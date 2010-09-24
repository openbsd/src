#!./perl -w

# Test the well-formed-ness of the MANIFEST file.

BEGIN {
    chdir 't';
    @INC = '../lib';
}

use strict;
use File::Spec;
require './test.pl';

plan('no_plan');

my $manifest = File::Spec->catfile(File::Spec->updir(), 'MANIFEST');

open my $m, '<', $manifest or die "Can't open '$manifest': $!";

# Test that MANIFEST uses tabs - not spaces - after the name of the file.
while (<$m>) {
    chomp;
    next unless /\s/;   # Ignore lines without whitespace (i.e., filename only)
    my ($file, $separator) = /^(\S+)(\s+)/;
    isnt($file, undef, "Line $. doesn't start with a blank") or next;
    # Remember, we're running from t/
    ok(-f "../$file", "File $file exists");
    if ($separator !~ tr/\t//c) {
	# It's all tabs
	next;
    } elsif ($separator !~ tr/ //c) {
	# It's all spaces
	fail("Spaces in entry for $file");
    } elsif ($separator =~ tr/\t//) {
	fail("Mixed tabs and spaces in entry for $file");
    } else {
	fail("Odd whitespace in entry for $file");
    }
}

close $m or die $!;

# Test that MANIFEST is properly sorted
SKIP: {
    skip("'Porting/manisort' not found", 1) if (! -f '../Porting/manisort');

    my $result = runperl('progfile' => '../Porting/manisort',
                         'args'     => [ '-c', '../MANIFEST' ],
                         'stderr'   => 1);

    like($result, qr/is sorted properly/, 'MANIFEST sorted properly');
}

# EOF
