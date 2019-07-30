use strict;

my %VERSION;

if (open(PATCHLEVEL_H, "patchlevel.h")) {
  while (<PATCHLEVEL_H>) {
     if (/#define\s+PERL_(REVISION|VERSION|SUBVERSION)\s+(\d+)/) {
         $VERSION{$1} = $2;
     }
  }
  close PATCHLEVEL_H;
} else {
  die "$0: patchlevel.h: $!\n";
}

die "$0: Perl release looks funny.\n"
  unless (defined $VERSION{REVISION} && $VERSION{REVISION} == 5 &&
          defined $VERSION{VERSION}  && $VERSION{VERSION}  >= 8 &&
          defined $VERSION{SUBVERSION});


\%VERSION;
