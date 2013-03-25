#!./perl -w

=head1 filenames.t

Test the well-formed-ness of filenames names in the MANIFEST file. Current
tests being done:

=over 4

=item * no more than 39 characters before the dot, and 39 after

=item * no filenames starting with -

=item * don't use any of these names (regardless of case) before the dot: CON,
PRN, AUX, NUL, COM1, COM2, COM3, COM4, COM5, COM6, COM7, COM8, COM9, LPT1,
LPT2, LPT3, LPT4, LPT5, LPT6, LPT7, LPT8, and LPT9

=item * no spaces, ( or & in filenames

=back

=cut

BEGIN {
    chdir 't';
    @INC = '../lib';
}

use strict;
use File::Spec;
use File::Basename;
require './test.pl';


my $manifest = File::Spec->catfile(File::Spec->updir(), 'MANIFEST');

open my $m, '<', $manifest or die "Can't open '$manifest': $!";
my @files;
while (<$m>) {
    chomp;
    my($path) = split /\t+/;
    push @files, $path;

}
close $m or die $!;

plan(scalar @files);

for my $file (@files) {
    validate_file_name($file);
}
exit 0;


sub validate_file_name {
    my $path = shift;
    my $filename = basename $path;

    note("testing $path");

    my @path_components = split('/',$path);
    pop @path_components; # throw away the filename
    for my $component (@path_components) {
	if ($component =~ /\./) {
	    fail("no directory components containing '.'");
	    return;
	}
	if (length $component > 32) {
	    fail("no directory with a name over 32 characters (VOS requirement)");
	    return;
	}
    }


    if ($filename =~ /^\-/) {
	fail("filename does not start with -");
	return;
    }

    my($before, $after) = split /\./, $filename;
    if (length $before > 39) {
	fail("filename has 39 or fewer characters before the dot");
	return;
    }
    if ($after) {
	if (length $after > 39) {
	    fail("filename has 39 or fewer characters after the dot");
	    return;
	}
    }

    if ($filename =~ /^(?:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])\./i) {
	fail("filename has a reserved name");
	return;
    }

    if ($filename =~ /\s|\(|\&/) {
	fail("filename has a reserved character");
	return;
    }
    pass("filename ok");
}

# EOF
