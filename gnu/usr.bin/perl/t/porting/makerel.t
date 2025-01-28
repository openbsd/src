#!./perl -w

# What does this test?
# This tests that files referenced in Porting/makerel exist
# This is important because otherwise the missing files will
# only be discovered when actually attempting a release
#
# It's broken - how do I fix it?
# EIther make sure that the file referenced by Porting/makerel exists
# or delete the line in Porting/makerel referencing that file

use Config;
BEGIN {
    @INC = '..' if -f '../TestInit.pm';
}
use TestInit qw(T); # T is chdir to the top level

require './t/test.pl';

plan('no_plan');

my $makerel = 'Porting/makerel';

open my $m, '<', $makerel or die "Can't open '$makerel': $!";
my @files;
while (<$m>) {
    if( /\@writables = /../\)/ ) {
        if( /^\s+(\S+)/ ) {
            my $file = $1;
            push @files, $file;
            ok(-f $file, "File $file exists");
        };
    }
}

close $m or die $!;

# EOF
