#!./perl

#
# test the logical operators '&&', '||', '!', 'and', 'or', 'not'
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 17;

for my $i (undef, 0 .. 2, "", "0 but true") {
    my $true = 1;
    my $false = 0;
    for my $j (undef, 0 .. 2, "", "0 but true") {
	$true &&= !(
	    ((!$i || !$j) != !($i && $j))
	    or (!($i || $j) != (!$i && !$j))
	    or (!!($i || $j) != !(!$i && !$j))
	    or (!(!$i || !$j) != !!($i && $j))
	);
	$false ||= (
	    ((!$i || !$j) == !!($i && $j))
	    and (!!($i || $j) == (!$i && !$j))
	    and ((!$i || $j) == ($i && !$j))
	    and (($i || !$j) != (!$i && $j))
	);
    }
    my $m = ! defined $i ? 'undef'
       : $i eq ''   ? 'empty string'
       : $i;
    ok( $true, "true: $m");
    ok( ! $false, "false: $m");
}

my $i = 0;
(($i ||= 1) &&= 3) += 4;
is( $i, 7, '||=, &&=');

my ($x, $y) = (1, 8);
$i = !$x || $y;
is( $i, 8, 'negation precedence with ||' );

++$y;
$i = !$x || !$x || !$x || $y;
is( $i, 9, 'negation precedence with ||, multiple operands' );

$x = 0;
++$y;
$i = !$x && $y;
is( $i, 10, 'negation precedence with &&' );

++$y;
$i = !$x && !$x && !$x && $y;
is( $i, 11, 'negation precedence with &&, multiple operands' );
