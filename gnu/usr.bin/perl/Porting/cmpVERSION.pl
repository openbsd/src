#!/usr/bin/perl -w

#
# cmpVERSION - compare two Perl source trees for modules
# that have identical version numbers but different contents.
#
# Original by slaven@rezic.de, modified by jhi.
#

use strict;

use ExtUtils::MakeMaker;
use File::Compare;
use File::Find;
use File::Spec::Functions qw(rel2abs abs2rel catfile catdir curdir);

for (@ARGV[0, 1]) {
    die "$0: '$_' does not look like Perl directory\n"
	unless -f catfile($_, "perl.h") && -d catdir($_, "Porting");
}

my $dir2 = rel2abs($ARGV[1]);
chdir $ARGV[0] or die "$0: chdir '$ARGV[0]' failed: $!\n";

# Files to skip from the check for one reason or another,
# usually because they pull in their version from some other file.
my %skip;
@skip{'./lib/Exporter/Heavy.pm'} = ();

my @wanted;
find(
     sub { /\.pm$/ &&
	       ! exists $skip{$File::Find::name}
	       &&
	       do { my $file2 =
			catfile(catdir($dir2, $File::Find::dir), $_);
		    (my $xs_file1 = $_)     =~ s/\.pm$/.xs/;
		    (my $xs_file2 = $file2) =~ s/\.pm$/.xs/;
		    if (-e $xs_file1 && -e $xs_file2) {
			return if compare($_, $file2) == 0 &&
			          compare($xs_file1, $xs_file2) == 0;
		    } else {
			return if compare($_, $file2) == 0;
		    }
		    my $version1 = eval {MM->parse_version($_)};
		    my $version2 = eval {MM->parse_version($file2)};
		    push @wanted, $File::Find::name
			if defined $version1 &&
			   defined $version2 &&
                           $version1 eq $version2
		} }, curdir);
print map { $_, "\n" } sort @wanted;

