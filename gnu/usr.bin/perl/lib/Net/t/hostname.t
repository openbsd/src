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

use Net::Domain qw(hostname domainname hostdomain hostfqdn);
use Net::Config;

unless($NetConfig{test_hosts}) {
    print "1..0\n";
    exit 0;
}

print "1..5\n";

$domain = domainname();

if(defined $domain && $domain ne "") {
 print "ok 1 - defined, non-empty domainname\n";
}
elsif (not defined $domain) {
 print "ok 1 # SKIP domain not fully defined\n";
}
else {
 print "not ok 1\n";
}

# This checks thats hostanme does not overwrite $_
my @domain = qw(foo.example.com bar.example.jp);
my @copy = @domain;

my @dummy = grep { defined hostname() and hostname() eq $_ } @domain;

($domain[0] && $domain[0] eq $copy[0])
  ? print "ok 2\n"
  : print "not ok 2\n";

@dummy = grep { defined hostdomain() and hostdomain() eq $_ } @domain;

($domain[0] && $domain[0] eq $copy[0])
  ? print "ok 3\n"
  : print "not ok 3\n";

my $name = hostname();
my $domain = hostdomain();
if(defined $domain && defined $name && $name ne "" && $domain ne "") {
    hostfqdn() eq $name . "." . $domain ? print "ok 4\n" : print "not ok 4\n";
    domainname() eq $name . "." . $domain ? print "ok 5\n" : print "not ok 5\n";} else {
    print "ok 4 # SKIP domain not fully defined\n";
    print "ok 5 # SKIP domain not fully defined\n";
}
