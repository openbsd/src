#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..1\n";

use File::Find;

# hope we will eventually find ourself
find(sub { print "ok 1\n" if $_ eq 'filefind.t'; }, ".");
