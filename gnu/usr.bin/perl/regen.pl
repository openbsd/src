#!/usr/bin/perl -w
#
# regen.pl - a wrapper that runs all *.pl scripts to to autogenerate files

require 5.004;	# keep this compatible, an old perl is all we may have before
                # we build the new one

# The idea is to move the regen_headers target out of the Makefile so that
# it is possible to rebuild the headers before the Makefile is available.
# (and the Makefile is unavailable until after Configure is run, and we may
# wish to make a clean source tree but with current headers without running
# anything else.

use strict;

# Which scripts to run.

my @scripts = qw(
mg_vtable.pl
opcode.pl
overload.pl
reentr.pl
regcomp.pl
warnings.pl
embed.pl
feature.pl
);

my $tap = $ARGV[0] && $ARGV[0] eq '--tap' ? '# ' : '';
foreach my $pl (map {"regen/$_"} @scripts) {
  my @command =  ($^X, $pl, @ARGV);
  print "$tap@command\n";
  system @command;
}
