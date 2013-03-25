use strict;
use Test::More;

BEGIN { plan tests => 6 };

BEGIN { $ENV{PERL_JSON_BACKEND} = 0; }

BEGIN {
    use lib qw(t);
    use _unicode_handling;
}


use JSON::PP;


my $data = ["\x{3042}\x{3044}\x{3046}\x{3048}\x{304a}",
            "\x{304b}\x{304d}\x{304f}\x{3051}\x{3053}"];

my $j = new JSON::PP;
my $js = $j->encode($data);
$j = undef;

my @parts = (substr($js, 0, int(length($js) / 2)),
             substr($js, int(length($js) / 2)));
$j = JSON::PP->new;
my $object = $j->incr_parse($parts[0]);

ok( !defined $object );

eval {
    $j->incr_text;
};

like( $@, qr/incr_text can not be called when the incremental parser already started parsing/ );

$object = $j->incr_parse($parts[1]);

ok( defined $object );

is( $object->[0], $data->[0] );
is( $object->[1], $data->[1] );

eval {
    $j->incr_text;
};

ok( !$@ );

