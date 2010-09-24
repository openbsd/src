#!/usr/bin/perl
# $File: /member/local/autrijus/encoding-warnings/t/2-fatal.t $ $Author: autrijus $
# $Revision: #4 $ $Change: 1626 $ $DateTime: 2004-03-14T16:53:19.351256Z $

BEGIN {
    unless (eval { require Encode } ) {
	print "1..0 # Skip: no Encode\n";
	exit 0;
    }
}

use Test;
BEGIN { plan tests => 2 }

use strict;
use encoding::warnings 'FATAL';
ok(encoding::warnings->VERSION);

if ($] < 5.008) {
    ok(1);
    exit;
}

my ($a, $b, $c, $ok);

$SIG{__DIE__} = sub {
    if ($_[0] =~ /upgraded/) { ok(1); exit }
};

utf8::encode($a = chr(20000));
$b = chr(20000);
$c = $a . $b;

ok($ok);
