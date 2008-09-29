#!perl -w

BEGIN {
    use Config;
    if ($Config{extensions} !~ /\bEncode\b/) {
	print "1..0 # Skip: no Encode\n";
	exit 0;
    }
}

use Config;

use Test::More $Config{useperlio}
    ? (tests => 1)
    : (skip_all => 'No PerlIO enabled');

BEGIN {
    $SIG{__WARN__} = sub { $warn .= $_[0] };
}

# bug #41442
use PerlIO::encoding;
use open ':locale';
if ($warn !~ /Cannot find encoding/) {
    if (-e '/dev/null') { open STDERR, '>', '/dev/null' }
    warn "# \x{201e}\n"; # &bdquo;
}
ok(1); # we got that far
