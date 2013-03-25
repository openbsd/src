#!/usr/bin/perl

# Testing of basic document structures

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use Test::More tests => 6;
use CPAN::Meta::YAML;



ok defined &main::Load, 'Load is exported';
ok defined &main::Dump, 'Dump is exported';
ok not(defined &main::LoadFile), 'Load is exported';
ok not(defined &main::DumpFile), 'Dump is exported';

ok \&main::Load == \&CPAN::Meta::YAML::Load, 'Load is CPAN::Meta::YAML';
ok \&main::Dump == \&CPAN::Meta::YAML::Dump, 'Dump is CPAN::Meta::YAML';
