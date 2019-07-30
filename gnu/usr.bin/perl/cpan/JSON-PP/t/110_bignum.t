
use strict;
use Test::More;
BEGIN { plan tests => 6 };

BEGIN { $ENV{PERL_JSON_BACKEND} = 0; }

use JSON::PP;

eval q| require Math::BigInt |;

SKIP: {
    skip "Can't load Math::BigInt.", 6 if ($@);

    my $v = Math::BigInt->VERSION;
    $v =~ s/_.+$// if $v;

my $fix =  !$v       ? '+'
          : $v < 1.6 ? '+'
          : '';


my $json = new JSON::PP;

$json->allow_nonref->allow_bignum(1);
$json->convert_blessed->allow_blessed;

my $num  = $json->decode(q|100000000000000000000000000000000000000|);

isa_ok($num, 'Math::BigInt');
is("$num", $fix . '100000000000000000000000000000000000000');
is($json->encode($num), $fix . '100000000000000000000000000000000000000');

$num  = $json->decode(q|2.0000000000000000001|);

isa_ok($num, 'Math::BigFloat');
is("$num", '2.0000000000000000001');
is($json->encode($num), '2.0000000000000000001');


}
