#!/usr/bin/perl -w
require 5.003;	# keep this compatible, an old perl is all we may have before
                # we build the new one

# The idea is to move the regen_headers target out of the Makefile so that
# it is possible to rebuild the headers before the Makefile is available.
# (and the Makefile is unavailable until after Configure is run, and we may
# wish to make a clean source tree but with current headers without running
# anything else.

use strict;
my $perl = $^X;

require 'regen_lib.pl';
# keep warnings.pl in sync with the CPAN distribution by not requiring core
# changes
safer_unlink ("warnings.h", "lib/warnings.pm");

my %gen = (
	   'autodoc.pl'  => [qw[pod/perlapi.pod pod/perlintern.pod]],
	   'bytecode.pl' => [qw[ext/ByteLoader/byterun.h
				ext/ByteLoader/byterun.c
				ext/B/B/Asmdata.pm]],
	   'embed.pl'    => [qw[proto.h embed.h embedvar.h global.sym
				perlapi.h perlapi.c]],
	   'keywords.pl' => [qw[keywords.h]],
	   'opcode.pl'   => [qw[opcode.h opnames.h pp_proto.h pp.sym]],
	   'regcomp.pl'  => [qw[regnodes.h]],
	   'warnings.pl' => [qw[warnings.h lib/warnings.pm]],
	   'reentr.pl' => [qw[reentr.c reentr.h]],
	   );

sub do_cksum {
    my $pl = shift;
    my %cksum;
    for my $f (@{ $gen{$pl} }) {
	local *FH;
	if (open(FH, $f)) {
	    local $/;
	    $cksum{$f} = unpack("%32C*", <FH>);
	    close FH;
	} else {
	    warn "$0: $f: $!\n";
	}
    }
    return %cksum;
}

foreach my $pl (qw (keywords.pl opcode.pl embed.pl bytecode.pl
		    regcomp.pl warnings.pl autodoc.pl reentr.pl)) {
  print "$^X $pl\n";
  my %cksum0;
  %cksum0 = do_cksum($pl) unless $pl eq 'warnings.pl'; # the files were removed
  system "$^X $pl";
  next if $pl eq 'warnings.pl'; # the files were removed
  my %cksum1 = do_cksum($pl);
  my @chg;
  for my $f (@{ $gen{$pl} }) {
      push(@chg, $f)
	  if !defined($cksum0{$f}) ||
	     !defined($cksum1{$f}) ||
	     $cksum0{$f} ne $cksum1{$f};
  }
  print "Changed: @chg\n" if @chg;
}
