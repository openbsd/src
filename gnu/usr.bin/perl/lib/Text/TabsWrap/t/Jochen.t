#!/usr/bin/perl -I.

use Text::Wrap;

print "1..1\n";

$Text::Wrap::columns = 1;
eval { wrap('', '', ''); };

print $@ ? "not ok 1\n" : "ok 1\n";

