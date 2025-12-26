#!./perl

use strict;
use warnings;

$|=1;
use Test::More tests => 3;;
use Scalar::Util qw(isvstring);

my $vs = ord("A") == 193 ? 241.75.240 : 49.46.48;

ok( $vs == "1.0", 'dotted num');
if ($] >= 5.008000) {
  ok( isvstring($vs), 'isvstring');
}
else {
  ok( !isvstring($vs), "isvstring is false for all values on $]");
}

my $sv = "1.0";
ok( !isvstring($sv), 'not isvstring');



