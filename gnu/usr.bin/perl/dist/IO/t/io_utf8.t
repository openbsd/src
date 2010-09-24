#!./perl

BEGIN {
    unless ($] >= 5.008 and find PerlIO::Layer 'perlio') {
	print "1..0 # Skip: not perlio\n";
	exit 0;
    }
}

require($ENV{PERL_CORE} ? "../../t/test.pl" : "./t/test.pl");

plan(tests => 5);

my $io;

use_ok('IO::File');

$io = IO::File->new;

ok($io->open("io_utf8", ">:utf8"), "open >:utf8");
ok((print $io chr(256)), "print chr(256)");
undef $io;

$io = IO::File->new;
ok($io->open("io_utf8", "<:utf8"), "open <:utf8");
is(ord(<$io>), 256, "readline chr(256)");
undef $io;

END {
  1 while unlink "io_utf8";
}
