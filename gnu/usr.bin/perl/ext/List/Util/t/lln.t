#!/usr/bin/perl -w
# -*- perl -*-


#
# $Id: lln.t,v 1.2 2003/12/03 03:02:31 millert Exp $
# Author: Slaven Rezic
#

use strict;
use vars qw(%Config);

BEGIN {
    unless (-d 'blib') {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Config; import Config;
	keys %Config; # Silence warning
	if ($Config{extensions} !~ /\bList\/Util\b/) {
	    print "1..0 # Skip: List::Util was not built\n";
	    exit 0;
	}
    }
}

use Scalar::Util qw(looks_like_number);

my $i;
sub ok { print +(($_[0] eq $_[1]) ? "": "not "), "ok ",++$i,"\n" }

print "1..12\n";

ok(!!looks_like_number("1"),	    1);
ok(!!looks_like_number("-1"),	    1);
ok(!!looks_like_number("+1"),	    1);
ok(!!looks_like_number("1.0"),	    1);
ok(!!looks_like_number("+1.0"),	    1);
ok(!!looks_like_number("-1.0"),	    1);
ok(!!looks_like_number("-1.0e-12"), 1);
ok(!!looks_like_number("Inf"),	    $] >= 5.006001);
ok(!!looks_like_number("Infinity"), $] >= 5.008);
ok(!!looks_like_number("NaN"),	    $] >= 5.008);
ok(!!looks_like_number("foo"),	    '');
ok(!!looks_like_number(undef),	    1);
# That's enough - we trust the perl core tests like t/base/num.t

__END__
