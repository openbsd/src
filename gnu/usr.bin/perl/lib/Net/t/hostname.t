#!./perl -w

BEGIN {
    unless (-d 'blib') {
	chdir 't' if -d 't';
	@INC = '../lib';
    }
    if (!eval "require Socket") {
	print "1..0 # no Socket\n"; exit 0;
    }
    if (ord('A') == 193 && !eval "require Convert::EBCDIC") {
        print "1..0 # EBCDIC but no Convert::EBCDIC\n"; exit 0;
    }
}

use Net::Domain qw(hostname domainname hostdomain);
use Net::Config;

unless($NetConfig{test_hosts}) {
    print "1..0\n";
    exit 0;
}

print "1..2\n";

$domain = domainname();

if(defined $domain && $domain ne "") {
 print "ok 1\n";
}
else {
 print "not ok 1\n";
}

# This check thats hostanme does not overwrite $_
my @domain = qw(foo.example.com bar.example.jp);
my @copy = @domain;

my @dummy = grep { hostname eq $_ } @domain;

($domain[0] && $domain[0] eq $copy[0])
  ? print "ok 2\n"
  : print "not ok 2\n";
