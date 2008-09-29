#!/usr/bin/perl
# $File: /member/local/autrijus/encoding-warnings//t/1-warning.t $ $Author: autrijus $
# $Revision: #5 $ $Change: 6145 $ $DateTime: 2004-07-16T03:49:06.717424Z $

BEGIN {
    unless (eval { require Encode } ) {
	print "1..0 # Skip: no Encode\n";
	exit 0;
    }
}

use Test;
BEGIN { plan tests => 2 }

use strict;
use encoding::warnings;
ok(encoding::warnings->VERSION);

if ($] < 5.008) {
    ok(1);
    exit;
}

my ($a, $b, $c, $ok);

$SIG{__WARN__} = sub {
    if ($_[0] =~ /upgraded/) { ok(1); exit }
};

utf8::encode($a = chr(20000));
$b = chr(20000);
$c = $a . $b;

ok($ok);

__END__
