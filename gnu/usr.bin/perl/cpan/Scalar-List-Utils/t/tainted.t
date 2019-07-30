#!./perl -T

use strict;
use warnings;

use Test::More tests => 5;

use Scalar::Util qw(tainted);

ok( !tainted(1), 'constant number');

my $var = 2;

ok( !tainted($var), 'known variable');

my $key = (grep { !/^PERL/ } keys %ENV)[0];

ok( tainted($ENV{$key}),	'environment variable');

$var = $ENV{$key};
ok( tainted($var),	'copy of environment variable');

{
    package Tainted;
    sub TIESCALAR { bless {} }
    sub FETCH { $^X }
}

tie my $tiedvar, 'Tainted';
ok( tainted($tiedvar), 'for magic variables');
