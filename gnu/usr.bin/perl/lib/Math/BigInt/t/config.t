#!/usr/bin/perl -w

use strict;
use Test;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib'; # for running manually
  plan tests => 51;
  } 

# test whether Math::BigInt->config() and Math::BigFloat->config() works

use Math::BigInt lib => 'Calc';
use Math::BigFloat;

my $mbi = 'Math::BigInt'; my $mbf = 'Math::BigFloat';

##############################################################################
# BigInt

ok ($mbi->can('config'));

my $cfg = $mbi->config();

ok (ref($cfg),'HASH');

ok ($cfg->{lib},'Math::BigInt::Calc');
ok ($cfg->{lib_version}, $Math::BigInt::Calc::VERSION);
ok ($cfg->{class},$mbi);
ok ($cfg->{upgrade}||'','');
ok ($cfg->{div_scale},40);

ok ($cfg->{precision}||0,0);	# should test for undef
ok ($cfg->{accuracy}||0,0);

ok ($cfg->{round_mode},'even');

ok ($cfg->{trap_nan},0);
ok ($cfg->{trap_inf},0);

##############################################################################
# BigFloat

ok ($mbf->can('config'));

$cfg = $mbf->config();

ok (ref($cfg),'HASH');

ok ($cfg->{lib},'Math::BigInt::Calc');
ok ($cfg->{with},'Math::BigInt::Calc');
ok ($cfg->{lib_version}, $Math::BigInt::Calc::VERSION);
ok ($cfg->{class},$mbf);
ok ($cfg->{upgrade}||'','');
ok ($cfg->{div_scale},40);

ok ($cfg->{precision}||0,0);	# should test for undef
ok ($cfg->{accuracy}||0,0);

ok ($cfg->{round_mode},'even');

ok ($cfg->{trap_nan},0);
ok ($cfg->{trap_inf},0);

##############################################################################
# test setting values

my $test = {
   trap_nan => 1, 
   trap_inf => 1, 
   accuracy => 2,
   precision => 3,
   round_mode => 'zero',
   div_scale => '100',
   upgrade => 'Math::BigInt::SomeClass',
   downgrade => 'Math::BigInt::SomeClass',
  };

my $c;

foreach my $key (keys %$test)
  {
  # see if setting in MBI works
  eval ( "$mbi\->config( $key => '$test->{$key}' );" );
  $c = $mbi->config(); ok ("$key = $c->{$key}", "$key = $test->{$key}");
  $c = $mbf->config(); 
  # see if setting it in MBI leaves MBF alone
  if (($c->{$key}||0) ne $test->{$key})
    {
    ok (1,1);
    }
  else
    {
    ok ("$key eq $c->{$key}","$key ne $test->{$key}");
    }

  # see if setting in MBF works
  eval ( "$mbf\->config( $key => '$test->{$key}' );" );
  $c = $mbf->config(); ok ("$key = $c->{$key}", "$key = $test->{$key}");
  }

##############################################################################
# test setting illegal keys (should croak)
  
$@ = ""; my $never_reached = 0;
eval ("$mbi\->config( 'some_garbage' => 1 ); $never_reached = 1;");
ok ($never_reached,0);

$@ = ""; $never_reached = 0;
eval ("$mbf\->config( 'some_garbage' => 1 ); $never_reached = 1;");
ok ($never_reached,0);

# this does not work. Why?
#ok ($@ eq "Illegal keys 'some_garbage' passed to Math::BigInt->config() at ./config.t line 104", 1);

# all tests done

