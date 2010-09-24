#!perl -w
use strict;

require './test.pl';

plan (tests => 24);
no warnings 'deprecated';

# Bug #27024
{
    # this used to segfault (because $[=1 is optimized away to a null block)
    my $x;
    $[ = 1 while $x;
    pass('#27204');
    $[ = 0; # restore the original value for less side-effects
}

# [perl #36313] perl -e "1for$[=0" crash
{
    my $x;
    $x = 1 for ($[) = 0;
    pass('optimized assignment to $[ used to segfault in list context');
    if ($[ = 0) { $x = 1 }
    pass('optimized assignment to $[ used to segfault in scalar context');
    $x = ($[=2.4);
    is($x, 2, 'scalar assignment to $[ behaves like other variables');
    $x = (($[) = 0);
    is($x, 1, 'list assignment to $[ behaves like other variables');
    $x = eval q{ ($[, $x) = (0) };
    like($@, qr/That use of \$\[ is unsupported/,
             'cannot assign to $[ in a list');
    eval q{ ($[) = (0, 1) };
    like($@, qr/That use of \$\[ is unsupported/,
             'cannot assign list of >1 elements to $[');
    eval q{ ($[) = () };
    like($@, qr/That use of \$\[ is unsupported/,
             'cannot assign list of <1 elements to $[');
}


{
    $[ = 11;
    cmp_ok($[ + 0, '==', 11, 'setting $[ affects $[');
    our $t11; BEGIN { $t11 = $^H{'$['} }
    cmp_ok($t11, '==', 11, 'setting $[ affects $^H{\'$[\'}');

    BEGIN { $^H{'$['} = 22 }
    cmp_ok($[ + 0, '==', 22, 'setting $^H{\'$\'} affects $[');
    our $t22; BEGIN { $t22 = $^H{'$['} }
    cmp_ok($t22, '==', 22, 'setting $^H{\'$[\'} affects $^H{\'$[\'}');

    BEGIN { %^H = () }
    my $val = do {
	no warnings 'uninitialized';
	$[;
    };
    cmp_ok($val, '==', 0, 'clearing %^H affects $[');
    our $t0; BEGIN { $t0 = $^H{'$['} }
    cmp_ok($t0, '==', 0, 'clearing %^H affects $^H{\'$[\'}');
}

{
    $[ = 13;
    BEGIN { $^H |= 0x04000000; $^H{foo} = "z"; }

    our($ri0, $rf0); BEGIN { $ri0 = $^H; $rf0 = $^H{foo}; }
    cmp_ok($[ + 0, '==', 13, '$[ correct before require');
    ok($ri0 & 0x04000000, '$^H correct before require');
    is($rf0, "z", '$^H{foo} correct before require');

    our($ra1, $ri1, $rf1, $rfe1);
    BEGIN { require "op/array_base.aux"; }
    cmp_ok($ra1, '==', 0, '$[ cleared for require');
    ok(!($ri1 & 0x04000000), '$^H cleared for require');
    is($rf1, undef, '$^H{foo} cleared for require');
    ok(!$rfe1, '$^H{foo} cleared for require');

    our($ri2, $rf2); BEGIN { $ri2 = $^H; $rf2 = $^H{foo}; }
    cmp_ok($[ + 0, '==', 13, '$[ correct after require');
    ok($ri2 & 0x04000000, '$^H correct after require');
    is($rf2, "z", '$^H{foo} correct after require');
}
