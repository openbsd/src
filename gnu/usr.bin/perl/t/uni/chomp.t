#!./perl -w

BEGIN {
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bEncode\b/) {
      print "1..0 # Skip: Encode was not built\n";
      exit 0;
    }
    if (ord("A") == 193) {
        print "1..0 # Skip: EBCDIC\n";
        exit 0;
    }
    unless (PerlIO::Layer->find('perlio')){
        print "1..0 # Skip: PerlIO required\n";
        exit 0;
    }
    if ($ENV{PERL_CORE_MINITEST}) {
        print "1..0 # Skip: no dynamic loading on miniperl, no Encode\n";
        exit 0;
    }

    require './test.pl';
}

use strict;
use Encode;

# %mbchars = (encoding => { bytes => utf8, ... }, ...);
# * pack('C*') is expected to return bytes even if ${^ENCODING} is true.
our %mbchars = (
    'big-5' => {
	pack('C*', 0x40)       => pack('U*', 0x40), # COMMERCIAL AT
	pack('C*', 0xA4, 0x40) => "\x{4E00}",       # CJK-4E00
    },
    'euc-jp' => {
	pack('C*', 0xB0, 0xA1)       => "\x{4E9C}", # CJK-4E9C
	pack('C*', 0x8F, 0xB0, 0xA1) => "\x{4E02}", # CJK-4E02
    },
    'shift-jis' => {
	pack('C*', 0xA9)       => "\x{FF69}", # halfwidth katakana small U
	pack('C*', 0x82, 0xA9) => "\x{304B}", # hiragana KA
    },
);

# 4 == @char; paired tests inside 3 nested loops,
# plus extra pair of tests in a loop, plus extra pair of tests.
plan tests => 2 * (4 ** 3 + 4 + 1) * (keys %mbchars);

for my $enc (sort keys %mbchars) {
    local ${^ENCODING} = find_encoding($enc);
    my @char = (sort(keys   %{ $mbchars{$enc} }),
		sort(values %{ $mbchars{$enc} }));

    for my $rs (@char) {
	local $/ = $rs;
	for my $start (@char) {
	    for my $end (@char) {
		my $string = $start.$end;
		my ($expect, $return);
		if ($end eq $rs) {
		    $expect = $start;
		    # The answer will always be a length in utf8, even if the
		    # scalar was encoded with a different length
		    $return = length ($end . "\x{100}") - 1;
		} else {
		    $expect = $string;
		    $return = 0;
		}
		is (chomp ($string), $return);
		is ($string, $expect); # "$enc \$/=$rs $start $end"
	    }
	}
	# chomp should not stringify references unless it decides to modify
	# them
	$_ = [];
	my $got = chomp();
	is ($got, 0);
	is (ref($_), "ARRAY", "chomp ref (no modify)");
    }

    $/ = ")";  # the last char of something like "ARRAY(0x80ff6e4)"
    my $got = chomp();
    is ($got, 1);
    ok (!ref($_), "chomp ref (modify)");
}
