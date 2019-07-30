#!perl -w

# Check that the current version of perl exists in Module-CoreList data

use TestInit qw(T);
use strict;
use Config;

require 't/test.pl';

plan(tests => 6);

use_ok('Module::CoreList');
use_ok('Module::CoreList::Utils');
use_ok('Module::CoreList::TieHashDelta');

{
  no warnings 'once';
  ok( defined $Module::CoreList::released{ $] }, "$] exists in released" );
  ok( defined $Module::CoreList::version{ $] }, "$] exists in version" );
  ok( defined $Module::CoreList::Utils::utilities{$] }, "$] exists in Utils" );
}
