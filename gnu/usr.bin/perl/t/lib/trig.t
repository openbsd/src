#!./perl 

#
# Regression tests for the Math::Trig package
#
# The tests are quite modest as the Math::Complex tests exercise
# these quite vigorously.
# 
# -- Jarkko Hietaniemi, April 1997

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Math::Trig;

use strict;

use vars qw($x $y $z);

my $eps = 1e-11;

sub near ($$;$) {
    abs($_[0] - $_[1]) < (defined $_[2] ? $_[2] : $eps);
}

print "1..7\n";

$x = 0.9;
print 'not ' unless (near(tan($x), sin($x) / cos($x)));
print "ok 1\n";

print 'not ' unless (near(sinh(2), 3.62686040784702));
print "ok 2\n";

print 'not ' unless (near(acsch(0.1), 2.99822295029797));
print "ok 3\n";

$x = asin(2);
print 'not ' unless (ref $x eq 'Math::Complex');
print "ok 4\n";

# avoid using Math::Complex here
$x =~ /^([^-]+)(-[^i]+)i$/;
($y, $z) = ($1, $2);
print 'not ' unless (near($y,  1.5707963267949) and
		     near($z, -1.31695789692482));
print "ok 5\n";

print 'not ' unless (near(deg2rad(90), pi/2));
print "ok 6\n";

print 'not ' unless (near(rad2deg(pi), 180));
print "ok 7\n";

# eof
