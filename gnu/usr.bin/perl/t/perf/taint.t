#!./perl -T
#
# All the tests in this file are ones that run exceptionally slowly
# (each test taking seconds or even minutes) in the absence of particular
# optimisations. Thus it is a sort of canary for optimisations being
# broken.
#
# Although it includes a watchdog timeout, this is set to a generous limit
# to allow for running on slow systems; therefore a broken optimisation
# might be indicated merely by this test file taking unusually long to
# run, rather than actually timing out.
#
# This is similar to t/perf/speed.t but tests performance regressions specific
# to taint.
#

BEGIN {
    chdir 't' if -d 't';
    @INC = ('../lib');
    require Config; import Config;
    require './test.pl';
}

use strict;
use warnings;
use Scalar::Util qw(tainted);

$| = 1;

plan tests => 2;

watchdog(60);

{
    my $in = substr($ENV{PATH}, 0, 0) . ( "ab" x 200_000 );
    utf8::upgrade($in);
    ok(tainted($in), "performance issue only when tainted");
    while ($in =~ /\Ga+b/g) { }
    pass("\\G on tainted string");
}

1;
