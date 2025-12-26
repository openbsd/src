#!./perl
BEGIN {
    chdir 't' if -d 't';
    require './test.pl';    # for fresh_perl_is() etc
    set_up_inc('../lib', '.', '../ext/re');
    require './charset_tools.pl';
    require './loc_tools.pl';
}
use Config;

use strict;
use warnings;

# This file is a place for tests that fail at the time they are added here.
#
# When a ticket is filed, just follow the paradigm(s) in this file to add a
# test that shows the failure.
#
# It is expected that when new tickets are opened, some will actually be
# duplicates of existing known bad behavior.  And since there are so many open
# tickets, we might overlook that.  If there is a test here, we would
# automatically discover that a fix for the newer ticket actually fixed an
# earlier one (or ones) as well.  Thus the issue can be closed, and the final
# disposition of the test here determined at that time.  (For example, perhaps
# it is redundant to the test demonstrating the bug that was intentionally
# fixed, so can be removed altogether.)

my $switches = "";

{   # Fixed by acababb42be12ff2986b73c1bfa963b70bb5d54e
    "abab" =~ /(?:[^b]*(?=(b)|(a))ab)*/;
    is($1, undef, "GH #16894");
}

our $TODO;
TODO: {
    local $TODO = "GH 16250";
    fresh_perl_is(<<~'EOF',
        "abcde5678" =~ / b (*pla:.*(*plb:(*plb:(.{4}))? (.{5})).$)/x;
        print $1 // "undef", ":", $2 // "undef", "\n";
        "abcde5678" =~ / b .* (*plb:(*plb:(.{4}))? (.{5}) ) .$ /x;
        print $1 // "undef", ":", $2 // "undef", "\n";
        EOF
    "undef:de567\nundef:de567", { eval $switches }, "");
}

TODO: {
    local $::TODO = 'GH 16876';
    fresh_perl('$_ = "a"; s{ x | (?{ s{}{x} }) }{}gx;',
               { stderr => 'devnull' });
    is($?, 0, "No assertion failure");
}

TODO: {
    local $::TODO = 'GH 16952';
    fresh_perl('s/d|(?{})!//.$&>0for$0,l..a0,0..0',
               { stderr => 'devnull' });
    is($?, 0, "No assertion failure");
}

TODO: {
    local $::TODO = 'GH 16971';
    fresh_perl('split(/00|0\G/, "000")',
               { stderr => 'devnull' });
    is($?, 0, "No assertion failure");
}

{
    fresh_perl('use re "eval";
                my @r;
                for$0(qw(0 0)){push@r,qr/@r(?{})/};',
               { stderr => 'devnull' });
    is($?, 0, "No assertion failure");
}

done_testing();
