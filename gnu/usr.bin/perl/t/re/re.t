#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;
use warnings;

use re qw(is_regexp regexp_pattern
          regname regnames regnames_count);
{
    my $qr=qr/foo/pi;
    my $rx = $$qr;

    ok(is_regexp($qr),'is_regexp(REGEXP ref)');
    ok(is_regexp($rx),'is_regexp(REGEXP)');
    ok(!is_regexp(''),'is_regexp("")');

    is((regexp_pattern($qr))[0],'foo','regexp_pattern[0] (ref)');
    is((regexp_pattern($qr))[1],'ip','regexp_pattern[1] (ref)');
    is(regexp_pattern($qr),'(?pi-xsm:foo)','scalar regexp_pattern (ref)');

    is((regexp_pattern($rx))[0],'foo','regexp_pattern[0] (bare REGEXP)');
    is((regexp_pattern($rx))[1],'ip','regexp_pattern[1] (bare REGEXP)');
    is(regexp_pattern($rx),'(?pi-xsm:foo)',
                                    'scalar regexp_pattern (bare REGEXP)');

    ok(!regexp_pattern(''),'!regexp_pattern("")');
}

if ('1234'=~/(?:(?<A>\d)|(?<C>!))(?<B>\d)(?<A>\d)(?<B>\d)/){
    my @names = sort +regnames();
    is("@names","A B","regnames");
    @names = sort +regnames(0);
    is("@names","A B","regnames");
    my $names = regnames();
    is($names, "B", "regnames in scalar context");
    @names = sort +regnames(1);
    is("@names","A B C","regnames");
    is(join("", @{regname("A",1)}),"13");
    is(join("", @{regname("B",1)}),"24");
    {
        if ('foobar'=~/(?<foo>foo)(?<bar>bar)/) {
            is(regnames_count(),2);
        } else {
            ok(0); ok(0);
        }
    }
    is(regnames_count(),3);
}

    { # Keep this test last, as whole script will be interrupted if times out
        # Bug #72998; this can loop 
        watchdog(2);
        eval '"\x{100}\x{FB00}" =~ /\x{100}\N{U+66}+/i';
        pass("Didn't loop");
    }

# New tests above this line, don't forget to update the test count below!
BEGIN { plan tests => 19 }
# No tests here!
