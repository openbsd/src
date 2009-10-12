################################################################################
#
#  $Revision: 1.2 $
#  $Author: millert $
#  $Date: 2009/10/12 18:24:31 $
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

my @modules = qw( IPC::SysV IPC::Msg IPC::Semaphore IPC::SharedMem );

eval 'use Pod::Coverage 0.10';
plan skip_all => "testing pod coverage requires Pod::Coverage 0.10" if $@;

eval 'use Test::Pod::Coverage 1.08';
plan skip_all => "testing pod coverage requires Test::Pod::Coverage 1.08" if $@;

plan tests => scalar @modules;

my $mod = shift @modules;
pod_coverage_ok($mod, { trustme => [qw( dl_load_flags )] }, "$mod is covered");

for my $mod (@modules) {
  pod_coverage_ok($mod, "$mod is covered");
}
