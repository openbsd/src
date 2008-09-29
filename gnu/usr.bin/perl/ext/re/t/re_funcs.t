#!./perl

BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Config;
	if (($Config::Config{'extensions'} !~ /\bre\b/) ){
        	print "1..0 # Skip -- Perl configured without re module\n";
		exit 0;
	}
}

use strict;

use Test::More; # test count at bottom of file
use re qw(is_regexp regexp_pattern regmust 
          regname regnames regnames_count);
{
    my $qr=qr/foo/pi;
    ok(is_regexp($qr),'is_regexp($qr)');
    ok(!is_regexp(''),'is_regexp("")');
    is((regexp_pattern($qr))[0],'foo','regexp_pattern[0]');
    is((regexp_pattern($qr))[1],'ip','regexp_pattern[1]');
    is(regexp_pattern($qr),'(?pi-xsm:foo)','scalar regexp_pattern');
    ok(!regexp_pattern(''),'!regexp_pattern("")');
}
{
    my $qr=qr/here .* there/x;
    my ($anchored,$floating)=regmust($qr);
    is($anchored,'here',"Regmust anchored - qr//");
    is($floating,'there',"Regmust floating - qr//");
    my $foo='blah';
    ($anchored,$floating)=regmust($foo);
    is($anchored,undef,"Regmust anchored - non ref");
    is($floating,undef,"Regmust anchored - non ref");
    my $bar=['blah'];
    ($anchored,$floating)=regmust($foo);
    is($anchored,undef,"Regmust anchored - ref");
    is($floating,undef,"Regmust anchored - ref");
}

if ('1234'=~/(?:(?<A>\d)|(?<C>!))(?<B>\d)(?<A>\d)(?<B>\d)/){
    my @names = sort +regnames();
    is("@names","A B","regnames");
    my @names = sort +regnames(0);
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
# New tests above this line, don't forget to update the test count below!
use Test::More tests => 20;
# No tests here!
