#!/usr/bin/perl -w

use strict;
use Test;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib'; # for running manually
  plan tests => 10;
  } 

# test whether Math::BigInt constant works

use Math::BigInt;

ok (Math::BigInt->can('config'));

my $cfg = Math::BigInt->config();

ok (ref($cfg),'HASH');

ok ($cfg->{lib},'Math::BigInt::Calc');
ok ($cfg->{lib_version}, $Math::BigInt::Calc::VERSION);
ok ($cfg->{class},'Math::BigInt');
ok ($cfg->{upgrade}||'','');
ok ($cfg->{div_scale},40);

ok ($cfg->{precision}||0,0);	# should test for undef
ok ($cfg->{accuracy}||0,0);

ok ($cfg->{round_mode},'even');

# all tests done

