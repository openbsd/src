#!/usr/bin/perl -w
use strict;
use Test::More tests => 4;

# For the moment, we'd like all our versions to be the same.
# In order to play nicely with some code scanners, they need to be
# hard-coded into the files, rather than just nicking the version
# from autodie::exception at run-time.

require Fatal;
require autodie;
require autodie::hints;
require autodie::exception;
require autodie::exception::system;

is($Fatal::VERSION, $autodie::VERSION);
is($autodie::VERSION, $autodie::exception::VERSION);
is($autodie::exception::VERSION, $autodie::exception::system::VERSION);
is($Fatal::VERSION, $autodie::hints::VERSION);
