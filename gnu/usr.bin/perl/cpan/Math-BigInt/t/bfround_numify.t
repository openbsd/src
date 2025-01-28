use strict;
use warnings;

use Test::More tests => 3;

use Math::BigFloat;

my $mbf = 'Math::BigFloat';

my $x = $mbf->new('123456.123456');

is($x->numify, 123456.123456, 'numify before bfround');

$x->bfround(-2);

is($x->numify, 123456.12, 'numify after bfround');
is($x->bstr, "123456.12", 'bstr after bfround');
