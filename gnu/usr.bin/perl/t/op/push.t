#!./perl

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

print "1..", 14 + 2*@tests, "\n";
die "blech" unless @tests;

@x = (1,2,3);
push(@x,@x);
if (join(':',@x) eq '1:2:3:1:2:3') {print "ok 1\n";} else {print "not ok 1\n";}
push(@x,4);
if (join(':',@x) eq '1:2:3:1:2:3:4') {print "ok 2\n";} else {print "not ok 2\n";}

# test for push/pop intuiting @ on array
{
    no warnings 'deprecated';
    push(x,3);
}
if (join(':',@x) eq '1:2:3:1:2:3:4:3') {print "ok 3\n";} else {print "not ok 3\n";}
{
    no warnings 'deprecated';
    pop(x);
}
if (join(':',@x) eq '1:2:3:1:2:3:4') {print "ok 4\n";} else {print "not ok 4\n";}

# test for push/pop on arrayref
push(\@x,5);
if (join(':',@x) eq '1:2:3:1:2:3:4:5') {print "ok 5\n";} else {print "not ok 5\n";}
pop(\@x);
if (join(':',@x) eq '1:2:3:1:2:3:4') {print "ok 6\n";} else {print "not ok 6\n";}

# test autovivification
push @$undef1, 1, 2, 3;
if (join(':',@$undef1) eq '1:2:3') {print "ok 7\n";} else {print "not ok 7\n";}

# test push on undef (error)
eval { push $undef2, 1, 2, 3 };
if ($@ =~ /Not an ARRAY/) {print "ok 8\n";} else {print "not ok 8\n";}

# test constant
use constant CONST_ARRAYREF => [qw/a b c/];
push CONST_ARRAYREF(), qw/d e f/;
if (join(':',@{CONST_ARRAYREF()}) eq 'a:b:c:d:e:f') {print "ok 9\n";} else {print "not ok 9\n";}

# test implicit dereference errors
eval "push 42, 0, 1, 2, 3";
if ( $@ && $@ =~ /must be array/ ) {print "ok 10\n"} else {print "not ok 10 # \$\@ = $@\n"}

$hashref = { };
eval { push $hashref, 0, 1, 2, 3 };
if ( $@ && $@ =~ /Not an ARRAY reference/ ) {print "ok 11\n"} else {print "not ok 11 # \$\@ = $@\n"}

eval { push bless([]), 0, 1, 2, 3 };
if ( $@ && $@ =~ /Not an unblessed ARRAY reference/ ) {print "ok 12\n"} else {print "not ok 12 # \$\@ = $@\n"}

$test = 13;

# test context
{
    my($first, $second) = ([1], [2]);
    sub two_things { return +($first, $second) }
    push two_things(), 3;
    if (join(':',@$first) eq '1' &&
        join(':',@$second) eq '2:3') {
        print "ok ",$test++,"\n";
    }
    else {
        print "not ok ",$test++," got: \$first = [ @$first ]; \$second = [ @$second ];\n";
    }

    push @{ two_things() }, 4;
    if (join(':',@$first) eq '1' &&
        join(':',@$second) eq '2:3:4') {
        print "ok ",$test++,"\n";
    }
    else {
        print "not ok ",$test++," got: \$first = [ @$first ]; \$second = [ @$second ];\n";
    }
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
    if (join(':',@got) eq join(':',@get) &&
	join(':',@x) eq join(':',@leave)) {
	print "ok ",$test++,"\n";
    }
    else {
	print "not ok ",$test++," got: @got == @get left: @x == @leave\n";
    }
    if (join(':',@got2) eq join(':',@get) &&
	join(':',@$y) eq join(':',@leave)) {
	print "ok ",$test++,"\n";
    }
    else {
	print "not ok ",$test++," got (arrayref): @got2 == @get left: @$y == @leave\n";
    }
}

1;  # this file is require'd by lib/tie-stdpush.t
