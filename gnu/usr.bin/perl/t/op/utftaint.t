#!./perl -T
# tests whether tainting works with UTF-8

BEGIN {
    if ($ENV{PERL_CORE_MINITEST}) {
        print "1..0 # Skip: no dynamic loading on miniperl, no threads\n";
        exit 0;
    }
    chdir 't' if -d 't';
    @INC = qw(../lib);
}

use strict;
use Config;

BEGIN {
    if ($Config{extensions} !~ m(\bList/Util\b)) {
        print "1..0 # Skip: no Scalar::Util module\n";
        exit 0;
    }
}

use Scalar::Util qw(tainted);

use Test;
plan tests => 3*10 + 3*8 + 2*16;
my $cnt = 0;

my $arg = $ENV{PATH}; # a tainted value
use constant UTF8 => "\x{1234}";

sub is_utf8 {
    my $s = shift;
    return 0xB6 != ord pack('a*', chr(0xB6).$s);
}

for my $ary ([ascii => 'perl'], [latin1 => "\xB6"], [utf8 => "\x{100}"]) {
    my $encode = $ary->[0];
    my $string = $ary->[1];

    my $taint = $arg; substr($taint, 0) = $ary->[1];

    print tainted($taint) == tainted($arg)
	? "ok " : "not ok ", ++$cnt, " # tainted: $encode, before test\n";

    my $lconcat = $taint;
       $lconcat .= UTF8;
    print $lconcat eq $string.UTF8
	? "ok " : "not ok ", ++$cnt, " # compare: $encode, concat left\n";

    print tainted($lconcat) == tainted($arg)
	? "ok " : "not ok ", ++$cnt, " # tainted: $encode, concat left\n";

    my $rconcat = UTF8;
       $rconcat .= $taint;
    print $rconcat eq UTF8.$string
	? "ok " : "not ok ", ++$cnt, " # compare: $encode, concat right\n";

    print tainted($rconcat) == tainted($arg)
	? "ok " : "not ok ", ++$cnt, " # tainted: $encode, concat right\n";

    my $ljoin = join('!', $taint, UTF8);
    print $ljoin eq join('!', $string, UTF8)
	? "ok " : "not ok ", ++$cnt, " # compare: $encode, join left\n";

    print tainted($ljoin) == tainted($arg)
	? "ok " : "not ok ", ++$cnt, " # tainted: $encode, join left\n";

    my $rjoin = join('!', UTF8, $taint);
    print $rjoin eq join('!', UTF8, $string)
	? "ok " : "not ok ", ++$cnt, " # compare: $encode, join right\n";

    print tainted($rjoin) == tainted($arg)
	? "ok " : "not ok ", ++$cnt, " # tainted: $encode, join right\n";

    print tainted($taint) == tainted($arg)
	? "ok " : "not ok ", ++$cnt, " # tainted: $encode, after test\n";
}


for my $ary ([ascii => 'perl'], [latin1 => "\xB6"], [utf8 => "\x{100}"]) {
    my $encode = $ary->[0];

    my $utf8 = pack('U*') . $ary->[1];
    my $byte = pack('C0a*', $utf8);

    my $taint = $arg; substr($taint, 0) = $utf8;
    utf8::encode($taint);

    print $taint eq $byte
	? "ok " : "not ok ", ++$cnt, " # compare: $encode, encode utf8\n";

    print pack('a*',$taint) eq pack('a*',$byte)
	? "ok " : "not ok ", ++$cnt, " # bytecmp: $encode, encode utf8\n";

    print !is_utf8($taint)
	? "ok " : "not ok ", ++$cnt, " # is_utf8: $encode, encode utf8\n";

    print tainted($taint) == tainted($arg)
	? "ok " : "not ok ", ++$cnt, " # tainted: $encode, encode utf8\n";

    my $taint = $arg; substr($taint, 0) = $byte;
    utf8::decode($taint);

    print $taint eq $utf8
	? "ok " : "not ok ", ++$cnt, " # compare: $encode, decode byte\n";

    print pack('a*',$taint) eq pack('a*',$utf8)
	? "ok " : "not ok ", ++$cnt, " # bytecmp: $encode, decode byte\n";

    print is_utf8($taint) eq ($encode ne 'ascii')
	? "ok " : "not ok ", ++$cnt, " # is_utf8: $encode, decode byte\n";

    print tainted($taint) == tainted($arg)
	? "ok " : "not ok ", ++$cnt, " # tainted: $encode, decode byte\n";
}


for my $ary ([ascii => 'perl'], [latin1 => "\xB6"]) {
    my $encode = $ary->[0];

    my $up   = pack('U*') . $ary->[1];
    my $down = pack('C0a*', $ary->[1]);

    my $taint = $arg; substr($taint, 0) = $up;
    utf8::upgrade($taint);

    print $taint eq $up
	? "ok " : "not ok ", ++$cnt, " # compare: $encode, upgrade up\n";

    print pack('a*',$taint) eq pack('a*',$up)
	? "ok " : "not ok ", ++$cnt, " # bytecmp: $encode, upgrade up\n";

    print is_utf8($taint)
	? "ok " : "not ok ", ++$cnt, " # is_utf8: $encode, upgrade up\n";

    print tainted($taint) == tainted($arg)
	? "ok " : "not ok ", ++$cnt, " # tainted: $encode, upgrade up\n";

    my $taint = $arg; substr($taint, 0) = $down;
    utf8::upgrade($taint);

    print $taint eq $up
	? "ok " : "not ok ", ++$cnt, " # compare: $encode, upgrade down\n";

    print pack('a*',$taint) eq pack('a*',$up)
	? "ok " : "not ok ", ++$cnt, " # bytecmp: $encode, upgrade down\n";

    print is_utf8($taint)
	? "ok " : "not ok ", ++$cnt, " # is_utf8: $encode, upgrade down\n";

    print tainted($taint) == tainted($arg)
	? "ok " : "not ok ", ++$cnt, " # tainted: $encode, upgrade down\n";

    my $taint = $arg; substr($taint, 0) = $up;
    utf8::downgrade($taint);

    print $taint eq $down
	? "ok " : "not ok ", ++$cnt, " # compare: $encode, downgrade up\n";

    print pack('a*',$taint) eq pack('a*',$down)
	? "ok " : "not ok ", ++$cnt, " # bytecmp: $encode, downgrade up\n";

    print !is_utf8($taint)
	? "ok " : "not ok ", ++$cnt, " # is_utf8: $encode, downgrade up\n";

    print tainted($taint) == tainted($arg)
	? "ok " : "not ok ", ++$cnt, " # tainted: $encode, downgrade up\n";

    my $taint = $arg; substr($taint, 0) = $down;
    utf8::downgrade($taint);

    print $taint eq $down
	? "ok " : "not ok ", ++$cnt, " # compare: $encode, downgrade down\n";

    print pack('a*',$taint) eq pack('a*',$down)
	? "ok " : "not ok ", ++$cnt, " # bytecmp: $encode, downgrade down\n";

    print !is_utf8($taint)
	? "ok " : "not ok ", ++$cnt, " # is_utf8: $encode, downgrade down\n";

    print tainted($taint) == tainted($arg)
	? "ok " : "not ok ", ++$cnt, " # tainted: $encode, downgrade down\n";
}


