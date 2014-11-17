#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;

use vars qw(@ary %ary %hash);

plan 86;

ok !defined($a);

$a = 1+1;
ok defined($a);

undef $a;
ok !defined($a);

$a = "hi";
ok defined($a);

$a = $b;
ok !defined($a);

@ary = ("1arg");
$a = pop(@ary);
ok defined($a);
$a = pop(@ary);
ok !defined($a);

@ary = ("1arg");
$a = shift(@ary);
ok defined($a);
$a = shift(@ary);
ok !defined($a);

$ary{'foo'} = 'hi';
ok defined($ary{'foo'});
ok !defined($ary{'bar'});
undef $ary{'foo'};
ok !defined($ary{'foo'});

{
    no warnings 'deprecated';
    ok defined(@ary);
    ok defined(%ary);
}
ok %ary;
undef @ary;
{
    no warnings 'deprecated';
    ok !defined(@ary);
}
undef %ary;
{
    no warnings 'deprecated';
    ok !defined(%ary);
}
ok !%ary;
@ary = (1);
{
    no warnings 'deprecated';
    ok defined @ary;
}
%ary = (1,1);
{
    no warnings 'deprecated';
    ok defined %ary;
}
ok %ary;

sub foo { pass; 1 }

&foo || fail;

ok defined &foo;
undef &foo;
ok !defined(&foo);

eval { undef $1 };
like $@, qr/^Modification of a read/;

eval { $1 = undef };
like $@, qr/^Modification of a read/;

{
    require Tie::Hash;
    tie my %foo, 'Tie::StdHash';
    no warnings 'deprecated';
    ok defined %foo;
    %foo = ( a => 1 );
    ok defined %foo;
}

{
    require Tie::Array;
    tie my @foo, 'Tie::StdArray';
    no warnings 'deprecated';
    ok defined @foo;
    @foo = ( a => 1 );
    ok defined @foo;
}

{
    # [perl #17753] segfault when undef'ing unquoted string constant
    eval 'undef tcp';
    like $@, qr/^Can't modify constant item/;
}

# bugid 3096
# undefing a hash may free objects with destructors that then try to
# modify the hash. Ensure that the hash remains consistent

{
    my (%hash, %mirror);

    my $iters = 5;

    for (1..$iters) {
	$hash{"k$_"} = bless ["k$_"], 'X';
	$mirror{"k$_"} = "k$_";
    }


    my $c = $iters;
    my $events;

    sub X::DESTROY {
	my $key = $_[0][0];
	$events .= 'D';
	note("----- DELETE($key) ------");
	delete $mirror{$key};

	is join('-', sort keys %hash), join('-', sort keys %mirror),
	    "$key: keys";
	is join('-', sort map $_->[0], values %hash),
	    join('-', sort values %mirror), "$key: values";

	# don't know exactly what we'll get from the iterator, but
	# it must be a sensible value
	my ($k, $v) = each %hash;
	ok defined $k ? exists($mirror{$k}) : (keys(%mirror) == 0),
	    "$key: each 1";

	is delete $hash{$key}, undef, "$key: delete";
	($k, $v) = each %hash;
	ok defined $k ? exists($mirror{$k}) : (keys(%mirror) <= 1),
	    "$key: each 2";

	$c++;
	if ($c <= $iters * 2) {
	    $hash{"k$c"} = bless ["k$c"], 'X';
	    $mirror{"k$c"} = "k$c";
	}
	$events .= 'E';
    }

    each %hash; # set eiter
    undef %hash;

    is scalar keys %hash, 0, "hash empty at end";
    is $events, ('DE' x ($iters*2)), "events";
    my ($k, $v) = each %hash;
    is $k, undef, 'each undef at end';
}

# part of #105906: inlined undef constant getting copied
BEGIN { $::{z} = \undef }
for (z,z) {
    push @_, \$_;
}
is $_[0], $_[1], 'undef constants preserve identity';

# this will segfault if it fails

sub PVBM () { 'foo' }
{ my $dummy = index 'foo', PVBM }

my $pvbm = PVBM;
undef $pvbm;
ok !defined $pvbm;
