#!perl -w
# Test overloading

use Test::More tests => 3;
use strict;

BEGIN {use_ok( "File::Temp" ); }

my $fh = new File::Temp();
ok( "$fh" ne "foo", "compare stringified object with string");
ok( $fh ne "foo", "compare object with string");