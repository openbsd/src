#!/usr/bin/perl -I.

#Causes Text::Wrap to die...
use warnings;
use strict;
use Text::Wrap;

my $toPrint = "(1) Category\t(2 or greater) New Category\n\n"; 
my $good =    "(1) Category\t(2 or greater) New Category\n"; 

my $toprint;

print "1..6\n";

local($Text::Wrap::break) = '\s';
eval { $toPrint = wrap("","",$toPrint); };
print $@ ? "not ok 1\n" : "ok 1\n";
print $toPrint eq $good ? "ok 2\n" : "not ok 2\n";

local($Text::Wrap::break) = '\d';
eval { $toPrint = wrap("","",$toPrint); };
print $@ ? "not ok 3\n" : "ok 3\n";
print $toPrint eq $good ? "ok 4\n" : "not ok 4\n";

local($Text::Wrap::break) = 'a';
eval { $toPrint = wrap("","",$toPrint); };
print $@ ? "not ok 5\n" : "ok 5\n";
print $toPrint eq $good ? "ok 6\n" : "not ok 6\n";

