#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;

plan tests => 3;

my %h;

ok (!Internals::HvREHASH(%h), "hash doesn't start with rehash flag on");

foreach (1..10) {
  $h{"\0"x$_}++;
}

ok (!Internals::HvREHASH(%h), "10 entries doesn't trigger rehash");

foreach (11..20) {
  $h{"\0"x$_}++;
}

ok (Internals::HvREHASH(%h), "20 entries triggers rehash");
