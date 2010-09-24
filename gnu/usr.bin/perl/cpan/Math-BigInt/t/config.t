#!/usr/bin/perl -w

use strict;
use Test::More;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib'; # for running manually
  plan tests => 55;
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

is ($cfg->{lib},'Math::BigInt::Calc', 'lib');
is ($cfg->{lib_version}, $Math::BigInt::Calc::VERSION, 'lib_version');
is ($cfg->{class},$mbi,'class');
is ($cfg->{upgrade}||'','', 'upgrade');
is ($cfg->{div_scale},40, 'div_Scale');

is ($cfg->{precision}||0,0, 'precision');	# should test for undef
is ($cfg->{accuracy}||0,0,'accuracy');
is ($cfg->{round_mode},'even','round_mode');

is ($cfg->{trap_nan},0, 'trap_nan');
is ($cfg->{trap_inf},0, 'trap_inf');

is ($mbi->config('lib'), 'Math::BigInt::Calc', 'config("lib")');

# can set via hash ref?
$cfg = $mbi->config( { trap_nan => 1 } );
is ($cfg->{trap_nan},1, 'can set via hash ref');

# reset for later
$mbi->config( trap_nan => 0 );

##############################################################################
# BigFloat

ok ($mbf->can('config'));

$cfg = $mbf->config();

ok (ref($cfg),'HASH');

is ($cfg->{lib},'Math::BigInt::Calc', 'lib');
is ($cfg->{with},'Math::BigInt::Calc', 'with');
is ($cfg->{lib_version}, $Math::BigInt::Calc::VERSION, 'lib_version');
is ($cfg->{class},$mbf,'class');
is ($cfg->{upgrade}||'','', 'upgrade');
is ($cfg->{div_scale},40, 'div_Scale');

is ($cfg->{precision}||0,0, 'precision');	# should test for undef
is ($cfg->{accuracy}||0,0,'accuracy');
is ($cfg->{round_mode},'even','round_mode');

is ($cfg->{trap_nan},0, 'trap_nan');
is ($cfg->{trap_inf},0, 'trap_inf');

is ($mbf->config('lib'), 'Math::BigInt::Calc', 'config("lib")');

# can set via hash ref?
$cfg = $mbf->config( { trap_nan => 1 } );
is ($cfg->{trap_nan},1, 'can set via hash ref');

# reset for later
$mbf->config( trap_nan => 0 );

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
    is (1,1);
    }
  else
    {
    is ("$key eq $c->{$key}","$key ne $test->{$key}", "$key");
    }

  # see if setting in MBF works
  eval ( "$mbf\->config( $key => '$test->{$key}' );" );
  $c = $mbf->config(); ok ("$key = $c->{$key}", "$key = $test->{$key}");
  }

##############################################################################
# test setting illegal keys (should croak)
  
$@ = ""; my $never_reached = 0;
eval ("$mbi\->config( 'some_garbage' => 1 ); $never_reached = 1;");
is ($never_reached,0);

$@ = ""; $never_reached = 0;
eval ("$mbf\->config( 'some_garbage' => 1 ); $never_reached = 1;");
is ($never_reached,0);

# this does not work. Why?
#ok ($@ eq "Illegal keys 'some_garbage' passed to Math::BigInt->config() at ./config.t line 104", 1);

# all tests done

