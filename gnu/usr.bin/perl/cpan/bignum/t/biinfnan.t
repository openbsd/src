#!perl

use strict;
use warnings;

use Test::More tests => 66;

use bigint;

#require "t/infnan.inc";

# The 'bigint'/'bignum'/'bigrat' pragma is lexical, so we can't 'require' or
# 'do' the included file. Slurp the whole thing and 'eval' it.

my $file = "t/infnan.inc";

open FILE, $file or die "$file: can't open file for reading: $!";
my $data = do { local $/; <FILE> };
close FILE or die "$file: can't close file after reading: $!";

eval $data;
die $@ if $@;
