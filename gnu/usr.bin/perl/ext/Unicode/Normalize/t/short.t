
BEGIN {
    unless ("A" eq pack('U', 0x41)) {
	print "1..0 # Unicode::Normalize " .
	    "cannot stringify a Unicode code point\n";
	exit 0;
    }
}

BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir('t') if -d 't';
        @INC = $^O eq 'MacOS' ? qw(::lib) : qw(../lib);
    }
}

#########################

use strict;
use Unicode::Normalize qw(:all);

print "1..8\n";
print "ok 1\n";

#########################

no warnings qw(utf8);

our $a = "\x{3042}"; # 3-byte length (in UTF-8/UTF-EBCDIC)
{
    use bytes;
    substr($a,1,length($a), ''); # remove trailing octets
}

print NFD($a) eq "\0"
   ? "ok" : "not ok", " 2\n";

print NFKD($a) eq "\0"
   ? "ok" : "not ok", " 3\n";

print NFC($a) eq "\0"
   ? "ok" : "not ok", " 4\n";

print NFKC($a) eq "\0"
   ? "ok" : "not ok", " 5\n";

print decompose($a) eq "\0"
   ? "ok" : "not ok", " 6\n";

print reorder($a) eq "\0"
   ? "ok" : "not ok", " 7\n";

print compose($a) eq "\0"
   ? "ok" : "not ok", " 8\n";

