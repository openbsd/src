#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

@tests = split(/\n/, <<EOF);
0 3,			0 1 2,		3 4 5 6 7
0 0 a b c,		,		a b c 0 1 2 3 4 5 6 7
8 0 a b c,		,		0 1 2 3 4 5 6 7 a b c
7 0 6.5,		,		0 1 2 3 4 5 6 6.5 7
1 0 a b c d e f g h i j,,		0 a b c d e f g h i j 1 2 3 4 5 6 7
0 1 a,			0,		a 1 2 3 4 5 6 7
1 6 x y z,		1 2 3 4 5 6,	0 x y z 7
0 7 x y z,		0 1 2 3 4 5 6,	x y z 7
1 7 x y z,		1 2 3 4 5 6 7,	0 x y z
4,			4 5 6 7,	0 1 2 3
-4,			4 5 6 7,	0 1 2 3
EOF

plan tests => 16 + @tests*4;
die "blech" unless @tests;

@x = (1,2,3);
push(@x,@x);
is( join(':',@x), '1:2:3:1:2:3', 'push array onto array');
push(@x,4);
is( join(':',@x), '1:2:3:1:2:3:4', 'push integer onto array');

# test for push/pop intuiting @ on array
{
    no warnings 'deprecated';
    push(x,3);
}
is( join(':',@x), '1:2:3:1:2:3:4:3', 'push intuiting @ on array');
{
    no warnings 'deprecated';
    pop(x);
}
is( join(':',@x), '1:2:3:1:2:3:4', 'pop intuiting @ on array');

# test for push/pop on arrayref
push(\@x,5);
is( join(':',@x), '1:2:3:1:2:3:4:5', 'push arrayref');
pop(\@x);
is( join(':',@x), '1:2:3:1:2:3:4', 'pop arrayref');

# test autovivification
push @$undef1, 1, 2, 3;
is( join(':',@$undef1), '1:2:3', 'autovivify array');

# test push on undef (error)
eval { push $undef2, 1, 2, 3 };
like( $@, qr/Not an ARRAY/, 'push on undef generates an error');

# test constant
use constant CONST_ARRAYREF => [qw/a b c/];
push CONST_ARRAYREF(), qw/d e f/;
is( join(':',@{CONST_ARRAYREF()}), 'a:b:c:d:e:f', 'test constant');

# test implicit dereference errors
eval "push 42, 0, 1, 2, 3";
like ( $@, qr/must be array/, 'push onto a literal integer');

$hashref = { };
eval { push $hashref, 0, 1, 2, 3 };
like( $@, qr/Not an ARRAY reference/, 'push onto a hashref');

eval { push bless([]), 0, 1, 2, 3 };
like( $@, qr/Not an unblessed ARRAY reference/, 'push onto a blessed array ref');

$test = 13;

# test context
{
    my($first, $second) = ([1], [2]);
    sub two_things { return +($first, $second) }
    push two_things(), 3;
    is( join(':',@$first), '1', "\$first = [ @$first ];");
    is( join(':',@$second), '2:3', "\$second = [ @$second ]");

    push @{ two_things() }, 4;
    is( join(':',@$first), '1', "\$first = [ @$first ];");
    is( join(':',@$second), '2:3:4', "\$second = [ @$second ]");
}

foreach $line (@tests) {
    ($list,$get,$leave) = split(/,\t*/,$line);
    ($pos, $len, @list) = split(' ',$list);
    @get = split(' ',$get);
    @leave = split(' ',$leave);
    @x = (0,1,2,3,4,5,6,7);
    $y = [0,1,2,3,4,5,6,7];
    if (defined $len) {
	@got = splice(@x, $pos, $len, @list);
	@got2 = splice($y, $pos, $len, @list);
    }
    else {
	@got = splice(@x, $pos);
	@got2 = splice($y, $pos);
    }
    is(join(':',@got), join(':',@get),   "got: @got == @get");
    is(join(':',@x),   join(':',@leave), "left: @x == @leave");
    is(join(':',@got2), join(':',@get),   "ref got: @got2 == @get");
    is(join(':',@$y),   join(':',@leave), "ref left: @$y == @leave");
}

1;  # this file is require'd by lib/tie-stdpush.t
