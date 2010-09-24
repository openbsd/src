#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}   

plan tests => 1368;

use strict;
use Config;

is(
    sprintf("%.40g ",0.01),
    sprintf("%.40g", 0.01)." ",
    q(the sprintf "%.<number>g" optimization)
);
is(
    sprintf("%.40f ",0.01),
    sprintf("%.40f", 0.01)." ",
    q(the sprintf "%.<number>f" optimization)
);

# cases of $i > 1 are against [perl #39126]
for my $i (1, 5, 10, 20, 50, 100) {
    chop(my $utf8_format = "%-*s\x{100}");
    my $string = "\xB4"x$i;        # latin1 ACUTE or ebcdic COPYRIGHT
    my $expect = $string."  "x$i;  # followed by 2*$i spaces
    is(sprintf($utf8_format, 3*$i, $string), $expect,
       "width calculation under utf8 upgrade, length=$i");
}

# check simultaneous width & precision with wide characters
for my $i (1, 3, 5, 10) {
    my $string = "\x{0410}"x($i+10);   # cyrillic capital A
    my $expect = "\x{0410}"x$i;        # cut down to exactly $i characters
    my $format = "%$i.${i}s";
    is(sprintf($format, $string), $expect,
       "width & precision interplay with utf8 strings, length=$i");
}

# Used to mangle PL_sv_undef
fresh_perl_like(
    'print sprintf "xxx%n\n"; print undef',
    'Modification of a read-only value attempted at - line 1\.',
    { switches => [ '-w' ] },
    q(%n should not be able to modify read-only constants),
);

# check overflows
for (int(~0/2+1), ~0, "9999999999999999999") {
    is(eval {sprintf "%${_}d", 0}, undef, "no sprintf result expected %${_}d");
    like($@, qr/^Integer overflow in format string for sprintf /, "overflow in sprintf");
    is(eval {printf "%${_}d\n", 0}, undef, "no printf result expected %${_}d");
    like($@, qr/^Integer overflow in format string for prtf /, "overflow in printf");
}

# check %NNN$ for range bounds
{
    my ($warn, $bad) = (0,0);
    local $SIG{__WARN__} = sub {
	if ($_[0] =~ /missing argument/i) {
	    $warn++
	}
	else {
	    $bad++
	}
    };

    my $fmt = join('', map("%$_\$s%" . ((1 << 31)-$_) . '$s', 1..20));
    my $result = sprintf $fmt, qw(a b c d);
    is($result, "abcd", "only four valid values in $fmt");
    is($warn, 36, "expected warnings");
    is($bad,   0, "unexpected warnings");
}

{
    foreach my $ord (0 .. 255) {
	my $bad = 0;
	local $SIG{__WARN__} = sub {
	    if ($_[0] !~ /^Invalid conversion in sprintf/) {
		warn $_[0];
		$bad++;
	    }
	};
	my $r = eval {sprintf '%v' . chr $ord};
	is ($bad, 0, "pattern '%v' . chr $ord");
    }
}

sub mysprintf_int_flags {
    my ($fmt, $num) = @_;
    die "wrong format $fmt" if $fmt !~ /^%([-+ 0]+)([1-9][0-9]*)d\z/;
    my $flag  = $1;
    my $width = $2;
    my $sign  = $num < 0 ? '-' :
		$flag =~ /\+/ ? '+' :
		$flag =~ /\ / ? ' ' :
		'';
    my $abs   = abs($num);
    my $padlen = $width - length($sign.$abs);
    return
	$flag =~ /0/ && $flag !~ /-/ # do zero padding
	    ? $sign . '0' x $padlen . $abs
	    : $flag =~ /-/ # left or right
		? $sign . $abs . ' ' x $padlen
		: ' ' x $padlen . $sign . $abs;
}

# Whole tests for "%4d" with 2 to 4 flags;
# total counts: 3 * (4**2 + 4**3 + 4**4) == 1008

my @flags = ("-", "+", " ", "0");
for my $num (0, -1, 1) {
    for my $f1 (@flags) {
	for my $f2 (@flags) {
	    for my $f3 ('', @flags) { # '' for doubled flags
		my $flag = $f1.$f2.$f3;
		my $width = 4;
		my $fmt   = '%'."${flag}${width}d";
		my $result = sprintf($fmt, $num);
		my $expect = mysprintf_int_flags($fmt, $num);
		is($result, $expect, qq/sprintf("$fmt",$num)/);

	        next if $f3 eq '';

		for my $f4 (@flags) { # quadrupled flags
		    my $flag = $f1.$f2.$f3.$f4;
		    my $fmt   = '%'."${flag}${width}d";
		    my $result = sprintf($fmt, $num);
		    my $expect = mysprintf_int_flags($fmt, $num);
		    is($result, $expect, qq/sprintf("$fmt",$num)/);
		}
	    }
	}
    }
}

# test that %f doesn't panic with +Inf, -Inf, NaN [perl #45383]
foreach my $n (2**1e100, -2**1e100, 2**1e100/2**1e100) { # +Inf, -Inf, NaN
    eval { my $f = sprintf("%f", $n); };
    is $@, "", "sprintf(\"%f\", $n)";
}

# test %ll formats with and without HAS_QUAD
eval { my $q = pack "q", 0 };
my $Q = $@ eq '';

my @tests = (
  [ '%lld' => [qw( 4294967296 -100000000000000 )] ],
  [ '%lli' => [qw( 4294967296 -100000000000000 )] ],
  [ '%llu' => [qw( 4294967296  100000000000000 )] ],
  [ '%Ld'  => [qw( 4294967296 -100000000000000 )] ],
  [ '%Li'  => [qw( 4294967296 -100000000000000 )] ],
  [ '%Lu'  => [qw( 4294967296  100000000000000 )] ],
);

for my $t (@tests) {
  my($fmt, $nums) = @$t;
  for my $num (@$nums) {
    my $w; local $SIG{__WARN__} = sub { $w = shift };
    is(sprintf($fmt, $num), $Q ? $num : $fmt, "quad: $fmt -> $num");
    like($w, $Q ? '' : qr/Invalid conversion in sprintf: "$fmt"/, "warning: $fmt");
  }
}

# Check unicode vs byte length
for my $width (1,2,3,4,5,6,7) {
    for my $precis (1,2,3,4,5,6,7) {
        my $v = "\x{20ac}\x{20ac}";
        my $format = "%" . $width . "." . $precis . "s";
        my $chars = ($precis > 2 ? 2 : $precis);
        my $space = ($width < 2 ? 0 : $width - $chars);
        fresh_perl_is(
            'my $v = "\x{20ac}\x{20ac}"; my $x = sprintf "'.$format.'", $v; $x =~ /^(\s*)(\S*)$/; print "$_" for map {length} $1, $2',
            "$space$chars",
            {},
            q(sprintf ").$format.q(", "\x{20ac}\x{20ac}"),
        );
    }
}
