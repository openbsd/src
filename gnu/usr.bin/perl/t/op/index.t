#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

require './test.pl';
plan( tests => 28 );

$foo = 'Now is the time for all good men to come to the aid of their country.';

$first = substr($foo,0,index($foo,'the'));
is($first, "Now is ");

$last = substr($foo,rindex($foo,'the'),100);
is($last, "their country.");

$last = substr($foo,index($foo,'Now'),2);
is($last, "No");

$last = substr($foo,rindex($foo,'Now'),2);
is($last, "No");

$last = substr($foo,index($foo,'.'),100);
is($last, ".");

$last = substr($foo,rindex($foo,'.'),100);
is($last, ".");

is(index("ababa","a",-1), 0);
is(index("ababa","a",0), 0);
is(index("ababa","a",1), 2);
is(index("ababa","a",2), 2);
is(index("ababa","a",3), 4);
is(index("ababa","a",4), 4);
is(index("ababa","a",5), -1);

is(rindex("ababa","a",-1), -1);
is(rindex("ababa","a",0), 0);
is(rindex("ababa","a",1), 0);
is(rindex("ababa","a",2), 2);
is(rindex("ababa","a",3), 2);
is(rindex("ababa","a",4), 4);
is(rindex("ababa","a",5), 4);

$a = "foo \x{1234}bar";

is(index($a, "\x{1234}"), 4);
is(index($a, "bar",    ), 5);

is(rindex($a, "\x{1234}"), 4);
is(rindex($a, "foo",    ), 0);

{
    my $needle = "\x{1230}\x{1270}";
    my @needles = split ( //, $needle );
    my $haystack = "\x{1228}\x{1228}\x{1230}\x{1270}";
    foreach ( @needles ) {
	my $a = index ( "\x{1228}\x{1228}\x{1230}\x{1270}", $_ );
	my $b = index ( $haystack, $_ );
	is($a, $b, q{[perl #22375] 'split'/'index' problem for utf8});
    }
    $needle = "\x{1270}\x{1230}"; # Transpose them.
    @needles = split ( //, $needle );
    foreach ( @needles ) {
	my $a = index ( "\x{1228}\x{1228}\x{1230}\x{1270}", $_ );
	my $b = index ( $haystack, $_ );
	is($a, $b, q{[perl #22375] 'split'/'index' problem for utf8});
    }
}
