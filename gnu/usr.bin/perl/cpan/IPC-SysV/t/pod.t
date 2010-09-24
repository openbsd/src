################################################################################
#
#  $Revision: 3 $
#  $Author: mhx $
#  $Date: 2007/10/13 19:07:53 +0200 $
#
################################################################################
#
#  Version 2.x, Copyright (C) 2007, Marcus Holland-Moritz <mhx@cpan.org>.
#  Version 1.x, Copyright (C) 1999, Graham Barr <gbarr@pobox.com>.
#
#  This program is free software; you can redistribute it and/or
#  modify it under the same terms as Perl itself.
#
################################################################################

BEGIN {
  if ($ENV{'PERL_CORE'}) {
    chdir 't' if -d 't';
    @INC = '../lib' if -d '../lib' && -d '../ext';
  }

  require Test::More; import Test::More;
  require Config; import Config;

  if ($ENV{'PERL_CORE'} && $Config{'extensions'} !~ m[\bIPC/SysV\b]) {
    plan(skip_all => 'IPC::SysV was not built');
  }
}

use strict;

my @pods;

# find all potential pod files
if (open F, "MANIFEST") {
  chomp(my @files = <F>);
  close F;
  for my $f (@files) {
    next if $f =~ /ppport/;
    if (open F, $f) {
      while (<F>) {
        if (/^=\w+/) {
          push @pods, $f;
          last;
        }
      }
      close F;
    }
  }
}

# load Test::Pod if possible, otherwise load Test::More
eval {
  require Test::Pod;
  $Test::Pod::VERSION >= 0.95
      or die "Test::Pod version only $Test::Pod::VERSION";
  import Test::Pod tests => scalar @pods;
};

if ($@) {
  require Test::More;
  import Test::More skip_all => "testing pod requires Test::Pod";
}
else {
  for my $pod (@pods) {
    pod_file_ok($pod);
  }
}

