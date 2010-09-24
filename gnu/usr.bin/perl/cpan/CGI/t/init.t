#!/usr/bin perl -w

use strict;
use Test::More tests => 1;

use CGI;


$_ = "abcdefghijklmnopq";
my $IN;
open ($IN, "t/init_test.txt");
my $q = CGI->new($IN);
is($_, 'abcdefghijklmnopq', 'make sure not to clobber $_ on init');
