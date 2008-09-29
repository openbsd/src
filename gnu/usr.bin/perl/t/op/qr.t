#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 2;

my $rx = qr//;

is(ref $rx, "Regexp", "qr// blessed into `Regexp' by default");

#
# DESTROY doesn't do anything in the case of qr// except make sure
# that lookups for it don't end up in AUTOLOAD lookups. But make sure
# it's there anyway.
#
ok($rx->can("DESTROY"), "DESTROY method defined for Regexp");
