#!/usr/bin/perl
use Test::More;
use File::Basename;
use File::Spec;
use strict;
my $THISDIR;
BEGIN {
   $THISDIR = dirname $0;
   unshift @INC, $THISDIR;
   eval {
     require "testpchk.pl";
     import TestPodChecker qw(testpodcheck);
   };
   warn $@ if $@;
};

my @pods;
unless($Pod::Checker::VERSION && $Pod::Checker::VERSION > 1.40) {
  plan skip_all => "we do not have a good Pod::Checker around";
} else {
  my $path = File::Spec->catfile($THISDIR,(File::Spec->updir()) x 2, 'lib', 'Pod', '*.pm');
  print "THISDIR=$THISDIR PATH=$path\n";
  @pods = glob($path);
  print "PODS=@pods\n";
  plan tests => scalar(@pods);
}

# test that our POD is correct!
my $errs = 0;

foreach my $pod (@pods) {
  my $out = File::Spec->catfile($THISDIR, basename($pod));
  $out =~ s{\.pm}{.OUT};
  my %options = ( -Out => $out );
  my $failmsg = testpodcheck(-In => $pod, -Out => $out, -Cmp => "$THISDIR/empty.xr");
  if($failmsg) {
    if(open(IN, "<$out")) {
      while(<IN>) {
        warn "podchecker: $_";
      }
      close(IN);
    } else {
      warn "Error: Cannot read output file $out: $!\n";
    }
    ok(0, $pod);
    $errs++;
  } else {
    ok(1, $pod);
  }
}

exit( ($errs == 0) ? 0 : -1 )  unless $ENV{HARNESS_ACTIVE};

