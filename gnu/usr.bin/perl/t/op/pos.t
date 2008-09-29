#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 6;

$x='banana';
$x=~/.a/g;
is(pos($x), 2);

$x=~/.z/gc;
is(pos($x), 2);

sub f { my $p=$_[0]; return $p }

$x=~/.a/g;
is(f(pos($x)), 4);

# Is pos() set inside //g? (bug id 19990615.008)
$x = "test string?"; $x =~ s/\w/pos($x)/eg;
is($x, "0123 5678910?");

$x = "123 56"; $x =~ / /g;
is(pos($x), 4);
{ local $x }
is(pos($x), 4);
