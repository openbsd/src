#!perl

use Test::More tests => 2;
use XS::APItest;

{
    my $vstr = v1.23.456;
    ok SvVOK($vstr), '$vstr has SvVOK';
    is SvVSTRING($vstr), "v1.23.456", 'SvVSTRING()';
}
