
BEGIN {
    unless ("A" eq pack('U', 0x41)) {
	print "1..0 # Unicode::Collate " .
	    "cannot stringify a Unicode code point\n";
	exit 0;
    }
    if ($ENV{PERL_CORE}) {
	chdir('t') if -d 't';
	@INC = $^O eq 'MacOS' ? qw(::lib) : qw(../lib);
    }
}

use strict;
use warnings;
BEGIN { $| = 1; print "1..52\n"; }
my $count = 0;
sub ok ($;$) {
    my $p = my $r = shift;
    if (@_) {
	my $x = shift;
	$p = !defined $x ? !defined $r : !defined $r ? 0 : $r eq $x;
    }
    print $p ? "ok" : "not ok", ' ', ++$count, "\n";
}

use Unicode::Collate::Locale;

ok(1);

#########################

my $Kssa = "\x{B95}\x{BCD}\x{BB7}";

my $objTa = Unicode::Collate::Locale->
    new(locale => 'TA', normalization => undef);

ok($objTa->getlocale, 'ta');

$objTa->change(level => 1);

ok($objTa->lt("\x{B94}", "\x{B82}"));
ok($objTa->gt("\x{B83}", "\x{B82}"));
ok($objTa->lt("\x{B83}", "\x{B95}\x{BCD}"));
ok($objTa->gt("\x{B95}", "\x{B95}\x{BCD}"));
ok($objTa->lt("\x{B95}", "\x{B99}\x{BCD}"));
ok($objTa->gt("\x{B99}", "\x{B99}\x{BCD}"));
ok($objTa->lt("\x{B99}", "\x{B9A}\x{BCD}"));
ok($objTa->gt("\x{B9A}", "\x{B9A}\x{BCD}"));
ok($objTa->lt("\x{B9A}", "\x{B9E}\x{BCD}"));
ok($objTa->gt("\x{B9E}", "\x{B9E}\x{BCD}"));
ok($objTa->lt("\x{B9E}", "\x{B9F}\x{BCD}"));
ok($objTa->gt("\x{B9F}", "\x{B9F}\x{BCD}"));
ok($objTa->lt("\x{B9F}", "\x{BA3}\x{BCD}"));
ok($objTa->gt("\x{BA3}", "\x{BA3}\x{BCD}"));
ok($objTa->lt("\x{BA3}", "\x{BA4}\x{BCD}"));
ok($objTa->gt("\x{BA4}", "\x{BA4}\x{BCD}"));
ok($objTa->lt("\x{BA4}", "\x{BA8}\x{BCD}"));
ok($objTa->gt("\x{BA8}", "\x{BA8}\x{BCD}"));
ok($objTa->lt("\x{BA8}", "\x{BAA}\x{BCD}"));
ok($objTa->gt("\x{BAA}", "\x{BAA}\x{BCD}"));
ok($objTa->lt("\x{BAA}", "\x{BAE}\x{BCD}"));
ok($objTa->gt("\x{BAE}", "\x{BAE}\x{BCD}"));
ok($objTa->lt("\x{BAE}", "\x{BAF}\x{BCD}"));
ok($objTa->gt("\x{BAF}", "\x{BAF}\x{BCD}"));
ok($objTa->lt("\x{BAF}", "\x{BB0}\x{BCD}"));
ok($objTa->gt("\x{BB0}", "\x{BB0}\x{BCD}"));
ok($objTa->lt("\x{BB0}", "\x{BB2}\x{BCD}"));
ok($objTa->gt("\x{BB2}", "\x{BB2}\x{BCD}"));
ok($objTa->lt("\x{BB2}", "\x{BB5}\x{BCD}"));
ok($objTa->gt("\x{BB5}", "\x{BB5}\x{BCD}"));
ok($objTa->lt("\x{BB5}", "\x{BB4}\x{BCD}"));
ok($objTa->gt("\x{BB4}", "\x{BB4}\x{BCD}"));
ok($objTa->lt("\x{BB4}", "\x{BB3}\x{BCD}"));
ok($objTa->gt("\x{BB3}", "\x{BB3}\x{BCD}"));
ok($objTa->lt("\x{BB3}", "\x{BB1}\x{BCD}"));
ok($objTa->gt("\x{BB1}", "\x{BB1}\x{BCD}"));
ok($objTa->lt("\x{BB1}", "\x{BA9}\x{BCD}"));
ok($objTa->gt("\x{BA9}", "\x{BA9}\x{BCD}"));
ok($objTa->lt("\x{BA9}", "\x{B9C}\x{BCD}"));
ok($objTa->gt("\x{B9C}", "\x{B9C}\x{BCD}"));
ok($objTa->lt("\x{B9C}", "\x{BB6}\x{BCD}"));
ok($objTa->gt("\x{BB6}", "\x{BB6}\x{BCD}"));
ok($objTa->lt("\x{BB6}", "\x{BB7}\x{BCD}"));
ok($objTa->gt("\x{BB7}", "\x{BB7}\x{BCD}"));
ok($objTa->lt("\x{BB7}", "\x{BB8}\x{BCD}"));
ok($objTa->gt("\x{BB8}", "\x{BB8}\x{BCD}"));
ok($objTa->lt("\x{BB8}", "\x{BB9}\x{BCD}"));
ok($objTa->gt("\x{BB9}", "\x{BB9}\x{BCD}"));
ok($objTa->lt("\x{BB9}", "${Kssa}\x{BCD}"));
ok($objTa->gt("${Kssa}", "${Kssa}\x{BCD}"));

# 52
