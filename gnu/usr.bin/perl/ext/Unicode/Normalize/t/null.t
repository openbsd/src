
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
use warnings;

use Unicode::Normalize qw(:all);
print "1..8\n";

print "ok 1\n";

# if $_ is not NULL-terminated, test may fail.

$_ = compose('abc');
print /c$/ ? "ok" : "not ok", " 2\n";

$_ = decompose('abc');
print /c$/ ? "ok" : "not ok", " 3\n";

$_ = reorder('abc');
print /c$/ ? "ok" : "not ok", " 4\n";

$_ = NFD('abc');
print /c$/ ? "ok" : "not ok", " 5\n";

$_ = NFC('abc');
print /c$/ ? "ok" : "not ok", " 6\n";

$_ = NFKD('abc');
print /c$/ ? "ok" : "not ok", " 7\n";

$_ = NFKC('abc');
print /c$/ ? "ok" : "not ok", " 8\n";

