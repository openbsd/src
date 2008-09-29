BEGIN {
    if ($ENV{PERL_CORE}) {
	chdir 't' if -d 't';
	@INC = '../lib';
    }
}
use strict;
use Test::More tests => 1;
use Attribute::Handlers;
# This had been failing since the introduction of proxy constant subroutines
use constant SETUP => undef;
sub Test : ATTR(CODE) { };
ok(1, "If we got here, CHECK didn't fail");
