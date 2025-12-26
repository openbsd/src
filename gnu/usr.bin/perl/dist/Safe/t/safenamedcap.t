use strict;
use Config;
use Test::More
    $] >= 5.010 && $Config{'extensions'} =~ /\bOpcode\b/
        ? (tests => 1)
        : (skip_all => "pre-5.10 perl or no Opcode extension");
use Safe;

BEGIN { Safe->new }
"foo" =~ /(?<foo>fo*)/;
is( $+{foo}, "foo", "Named capture works" );
