#!/usr/bin/perl -w

#
# Check that the various config.sh-clones have (at least) all the
# same symbols as the top-level config_h.SH so that the (potentially)
# needed symbols are not lagging after how Configure thinks the world
# is laid out.
#
# VMS is probably not handled properly here, due to their own
# rather elaborate DCL scripting.
#

use strict;

my $MASTER_CFG = "config_h.SH";
my %MASTER_CFG;

my @CFG = (
	   # This list contains both 5.8.x and 5.9.x files,
	   # we check from MANIFEST whether they are expected to be present.
	   # We can't base our check on $], because that's the version of the
	   # perl that we are running, not the version of the source tree.
	   "Cross/config.sh-arm-linux",
	   "epoc/config.sh",
	   "NetWare/config.wc",
	   "symbian/config.sh",
	   "uconfig.sh",
	   "plan9/config_sh.sample",
	   "vos/config.alpha.def",
	   "vos/config.ga.def",
	   "win32/config.bc",
	   "win32/config.gc",
	   "win32/config.vc",
	   "win32/config.vc64",
	   "win32/config.ce",
	   "configure.com",
	   "Porting/config.sh",
	  );

sub read_file {
    my ($fn, $sub) = @_;
    if (open(my $fh, $fn)) {
	local $_;
	while (<$fh>) {
	    &$sub;
	}
    } else {
	die "$0: Failed to open '$fn' for reading: $!\n";
    }
}

sub config_h_SH_reader {
    my $cfg = shift;
    return sub {
	while (/[^\\]\$([a-z]\w+)/g) {
	    my $v = $1;
	    next if $v =~ /^(CONFIG_H|CONFIG_SH)$/;
	    $cfg->{$v}++;
	}
    }
}

read_file($MASTER_CFG,
	  config_h_SH_reader(\%MASTER_CFG));

my %MANIFEST;

read_file("MANIFEST",
	  sub {
	      $MANIFEST{$1}++ if /^(.+?)\t/;
	  });

my @MASTER_CFG = sort keys %MASTER_CFG;

sub check_cfg {
    my ($fn, $cfg) = @_;
    for my $v (@MASTER_CFG) {
	print "$fn: missing '$v'\n" unless exists $cfg->{$v};
    }
}

for my $cfg (@CFG) {
    unless (exists $MANIFEST{$cfg}) {
	print STDERR "[skipping not-expected '$cfg']\n";
	next;
    }
    my %cfg;
    read_file($cfg,
	      sub {
		  return if /^\#/ || /^\s*$/ || /^\:/;
		  if ($cfg eq 'configure.com') {
		      s/(\s*!.*|\s*)$//; # remove trailing comments or whitespace
		      return if ! /^\$\s+WC "(\w+)='(.*)'"$/;
		  }
		  # foo='bar'
		  # foo=bar
		  # $foo='bar' # VOS 5.8.x specialty
		  # $foo=bar   # VOS 5.8.x specialty
		  if (/^\$?(\w+)='(.*)'$/) {
		      $cfg{$1}++;
		  }
		  elsif (/^\$?(\w+)=(.*)$/) {
		      $cfg{$1}++;
		  }
		  elsif (/^\$\s+WC "(\w+)='(.*)'"$/) {
		      $cfg{$1}++;
		  } else {
		      warn "$cfg:$.:$_";
		  }
	      });
    if ($cfg eq 'configure.com') {
	$cfg{startperl}++; # Cheat.
    }
    check_cfg($cfg, \%cfg);
}
