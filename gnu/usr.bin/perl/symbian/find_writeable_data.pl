#!/usr/bin/perl -w

#
# find_writeable_data - find non-const data in Symbian object files
#
# Use this when PETRAN tells you "dll has (un)initialised data".
# Expects to find the Symbian (GNU) nm in the executable path.
#
# Copyright (c) 2004-2005 Nokia.  All rights reserved.
#
# This utility is licensed under the same terms as Perl itself.
#

use strict;

BEGIN {
  unless (exists $ENV{EPOCROOT}) {
    die "$0: EPOCROOT unset\n";
  }
  if (open(my $fh, "nm --version |")) {
    unless (<$fh> =~ /^GNU nm .*-psion-.*/) {
      die "$0: Cannot find the GNU nm from Symbian\n";
    }
    close($fh);
  } else {
      die "$0: Cannot find any nm in the executable path: $!\n";
  }
  unless (@ARGV && $ARGV[0] =~ /\.mmp$/i) {
    die "$0: Must specify target mmp as the first argument\n";
  }
}

use Cwd;
use File::Basename;

my $dir = lc(getcwd());
my $tgt = basename(shift(@ARGV), ".mmp");

$dir =~ s!/!\\!g;
$dir =~ s!^c:!c:$ENV{EPOCROOT}epoc32\\build!;
$dir .= "\\$tgt\\thumb\\urel";

print $dir, "\n";

unless (-d $dir) {
  die "$0: No directory $dir\n";
}

my @o = glob("$dir\\*.o");

unless (@o) {
  die "$0: No objects in $dir\n";
}

for my $o (@o) {
  if (open(my $fh, "nm $o |")) {
    my @d;
    while (<$fh>) {
      next if / [TURtr] /;
      push @d, $_;
    }
    close($fh);
    if (@d) {
      $o =~ s!^\Q$dir\E\\!!;
      print "$o:\n";
      print @d;
    }
  } else {
    warn "$0: nm $o failed: $!\n";
  }
} 

exit(0);
