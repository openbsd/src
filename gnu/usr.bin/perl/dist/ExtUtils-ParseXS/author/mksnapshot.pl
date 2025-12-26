#!/usr/bin/perl
#
# mksnapshot.pl
#
# This is a very crude script to help check whether and/or what has
# changed in generated .c files as a result of changes to xsubpp and its
# modules.
#
# It finds all .xs files under the current directory, then copies to
# another directory, each associated .c file if it exists.
#
# The idea is that, while cd'ed to the base of the perl distribution, you
# run this script, which will copy all the generated .c files to one
# directory. You then rebuild perl with the newly modified xsubpp, and
# create a second snapshot to another directory, then compare the results.
#
# For example,
#
#    ... build perl...
#
#    $ mkdir /tmp/snap1 /tmp/snap2
#    $ dist/ExtUtils-ParseXS/author/mksnapshot.pl /tmp/snap1
#    $ git clean -xdf
#
#    ... modify xsubpp ...
#    ... build perl...
#
#    $ dist/ExtUtils-ParseXS/author/mksnapshot.pl /tmp/snap2
#    $ diff -ru /tmp/snap[12]
#
# Each snapped .c file is saved with each '/' component of its pathname
# changed to a '='. So ext/POSIX/POSIX.c would be copied to
#    /tmp/snap1/ext=POSIX=POSIX.c

use warnings;
use strict;

use File::Find;
use File::Copy;

die "usage: $0 snapdirectory\n" unless @ARGV == 1;
my $snapdir = shift @ARGV;

die "No such directory: $snapdir\n" unless -d $snapdir;

find(
    {
        wanted   => \&wanted,
        no_chdir => 1,
    },

    '.'
);



sub wanted {
    return unless /\.xs$/;
    my $f = $_;
    $f =~ s/\.xs$/.c/ or die;
    return unless -f $f;
    my $df = $f;
    $df =~ s{^/}{};
    $df =~ s{^\./}{};
    $df =~ s{/}{=}g;
    $df = "$snapdir/$df";
    print "snapping $f\n";
    copy($f, $df) or die "Can't copy $f to $df: $!\n";
}
