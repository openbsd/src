#!./perl -wT

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use File::Path;
use File::Spec::Functions;
use strict;

my $count = 0;
use warnings;

print "1..4\n";

# first check for stupid permissions second for full, so we clean up
# behind ourselves
for my $perm (0111,0777) {
    my $path = catdir(curdir(), "mhx", "bar");
    mkpath($path);
    chmod $perm, "mhx", $path;

    print "not " unless -d "mhx" && -d $path;
    print "ok ", ++$count, "\n";

    rmtree("mhx");
    print "not " if -e "mhx";
    print "ok ", ++$count, "\n";
}
