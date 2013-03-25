#!./perl

BEGIN {
    unless ($] >= 5.008 and find PerlIO::Layer 'perlio') {
	print "1..0 # Skip: not perlio\n";
	exit 0;
    }
    require($ENV{PERL_CORE} ? "../../t/test.pl" : "./t/test.pl");
}

use utf8;


plan(tests => 2);

open my $fh, ">:raw", 'io_utf8argv';
print $fh
   "\xce\x9c\xe1\xbd\xb7\xce\xb1\x20\xcf\x80\xe1\xbd\xb1\xcf\x80\xce".
   "\xb9\xce\xb1\x2c\x20\xce\xbc\xe1\xbd\xb0\x20\xcf\x80\xce\xbf\xce".
   "\xb9\xe1\xbd\xb0\x20\xcf\x80\xe1\xbd\xb1\xcf\x80\xce\xb9\xce\xb1".
   "\xcd\xbe\x0a";
close $fh or die "close: $!";


use open ":std", ":utf8";

use IO::Handle;

@ARGV = ('io_utf8argv') x 2;
is *ARGV->getline, "Μία πάπια, μὰ ποιὰ πάπια;\n",
  'getline respects open pragma when magically opening ARGV';

is join('',*ARGV->getlines), "Μία πάπια, μὰ ποιὰ πάπια;\n",
  'getlines respects open pragma when magically opening ARGV';

END {
  1 while unlink "io_utf8argv";
}
