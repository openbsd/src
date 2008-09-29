#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = ('.', '../lib');
}

require 'test.pl';

plan (125);

#
# @foo, @bar, and @ary are also used from tie-stdarray after tie-ing them
#

@ary = (1,2,3,4,5);
is(join('',@ary), '12345');

$tmp = $ary[$#ary]; --$#ary;
is($tmp, 5);
is($#ary, 3);
is(join('',@ary), '1234');

$[ = 1;
@ary = (1,2,3,4,5);
is(join('',@ary), '12345');

$tmp = $ary[$#ary]; --$#ary;
is($tmp, 5);
# Must do == here beacuse $[ isn't 0
ok($#ary == 4);
is(join('',@ary), '1234');

is($ary[5], undef);

$#ary += 1;	# see if element 5 gone for good
ok($#ary == 5);
ok(!defined $ary[5]);

$[ = 0;
@foo = ();
$r = join(',', $#foo, @foo);
is($r, "-1");
$foo[0] = '0';
$r = join(',', $#foo, @foo);
is($r, "0,0");
$foo[2] = '2';
$r = join(',', $#foo, @foo);
is($r, "2,0,,2");
@bar = ();
$bar[0] = '0';
$bar[1] = '1';
$r = join(',', $#bar, @bar);
is($r, "1,0,1");
@bar = ();
$r = join(',', $#bar, @bar);
is($r, "-1");
$bar[0] = '0';
$r = join(',', $#bar, @bar);
is($r, "0,0");
$bar[2] = '2';
$r = join(',', $#bar, @bar);
is($r, "2,0,,2");
reset 'b' if $^O ne 'VMS';
@bar = ();
$bar[0] = '0';
$r = join(',', $#bar, @bar);
is($r, "0,0");
$bar[2] = '2';
$r = join(',', $#bar, @bar);
is($r, "2,0,,2");

$foo = 'now is the time';
ok(scalar (($F1,$F2,$Etc) = ($foo =~ /^(\S+)\s+(\S+)\s*(.*)/)));
is($F1, 'now');
is($F2, 'is');
is($Etc, 'the time');

$foo = 'lskjdf';
ok(!($cnt = (($F1,$F2,$Etc) = ($foo =~ /^(\S+)\s+(\S+)\s*(.*)/))))
   or diag("$cnt $F1:$F2:$Etc");

%foo = ('blurfl','dyick','foo','bar','etc.','etc.');
%bar = %foo;
is($bar{'foo'}, 'bar');
%bar = ();
is($bar{'foo'}, undef);
(%bar,$a,$b) = (%foo,'how','now');
is($bar{'foo'}, 'bar');
is($bar{'how'}, 'now');
@bar{keys %foo} = values %foo;
is($bar{'foo'}, 'bar');
is($bar{'how'}, 'now');

@foo = grep(/e/,split(' ','now is the time for all good men to come to'));
is(join(' ',@foo), 'the time men come');

@foo = grep(!/e/,split(' ','now is the time for all good men to come to'));
is(join(' ',@foo), 'now is for all good to to');

$foo = join('',('a','b','c','d','e','f')[0..5]);
is($foo, 'abcdef');

$foo = join('',('a','b','c','d','e','f')[0..1]);
is($foo, 'ab');

$foo = join('',('a','b','c','d','e','f')[6]);
is($foo, '');

@foo = ('a','b','c','d','e','f')[0,2,4];
@bar = ('a','b','c','d','e','f')[1,3,5];
$foo = join('',(@foo,@bar)[0..5]);
is($foo, 'acebdf');

$foo = ('a','b','c','d','e','f')[0,2,4];
is($foo, 'e');

$foo = ('a','b','c','d','e','f')[1];
is($foo, 'b');

@foo = ( 'foo', 'bar', 'burbl');
push(foo, 'blah');
is($#foo, 3);

# various AASSIGN_COMMON checks (see newASSIGNOP() in op.c)

#curr_test(38);

@foo = @foo;
is("@foo", "foo bar burbl blah");				# 38

(undef,@foo) = @foo;
is("@foo", "bar burbl blah");					# 39

@foo = ('XXX',@foo, 'YYY');
is("@foo", "XXX bar burbl blah YYY");				# 40

@foo = @foo = qw(foo b\a\r bu\\rbl blah);
is("@foo", 'foo b\a\r bu\\rbl blah');				# 41

@bar = @foo = qw(foo bar);					# 42
is("@foo", "foo bar");
is("@bar", "foo bar");						# 43

# try the same with local
# XXX tie-stdarray fails the tests involving local, so we use
# different variable names to escape the 'tie'

@bee = ( 'foo', 'bar', 'burbl', 'blah');
{

    local @bee = @bee;
    is("@bee", "foo bar burbl blah");				# 44
    {
	local (undef,@bee) = @bee;
	is("@bee", "bar burbl blah");				# 45
	{
	    local @bee = ('XXX',@bee,'YYY');
	    is("@bee", "XXX bar burbl blah YYY");		# 46
	    {
		local @bee = local(@bee) = qw(foo bar burbl blah);
		is("@bee", "foo bar burbl blah");		# 47
		{
		    local (@bim) = local(@bee) = qw(foo bar);
		    is("@bee", "foo bar");			# 48
		    is("@bim", "foo bar");			# 49
		}
		is("@bee", "foo bar burbl blah");		# 50
	    }
	    is("@bee", "XXX bar burbl blah YYY");		# 51
	}
	is("@bee", "bar burbl blah");				# 52
    }
    is("@bee", "foo bar burbl blah");				# 53
}

# try the same with my
{
    my @bee = @bee;
    is("@bee", "foo bar burbl blah");				# 54
    {
	my (undef,@bee) = @bee;
	is("@bee", "bar burbl blah");				# 55
	{
	    my @bee = ('XXX',@bee,'YYY');
	    is("@bee", "XXX bar burbl blah YYY");		# 56
	    {
		my @bee = my @bee = qw(foo bar burbl blah);
		is("@bee", "foo bar burbl blah");		# 57
		{
		    my (@bim) = my(@bee) = qw(foo bar);
		    is("@bee", "foo bar");			# 58
		    is("@bim", "foo bar");			# 59
		}
		is("@bee", "foo bar burbl blah");		# 60
	    }
	    is("@bee", "XXX bar burbl blah YYY");		# 61
	}
	is("@bee", "bar burbl blah");				# 62
    }
    is("@bee", "foo bar burbl blah");				# 63
}

# try the same with our (except that previous values aren't restored)
{
    our @bee = @bee;
    is("@bee", "foo bar burbl blah");
    {
	our (undef,@bee) = @bee;
	is("@bee", "bar burbl blah");
	{
	    our @bee = ('XXX',@bee,'YYY');
	    is("@bee", "XXX bar burbl blah YYY");
	    {
		our @bee = our @bee = qw(foo bar burbl blah);
		is("@bee", "foo bar burbl blah");
		{
		    our (@bim) = our(@bee) = qw(foo bar);
		    is("@bee", "foo bar");
		    is("@bim", "foo bar");
		}
	    }
	}
    }
}

# make sure reification behaves
my $t = curr_test();
sub reify { $_[1] = $t++; print "@_\n"; }
reify('ok');
reify('ok');

curr_test($t);

# qw() is no longer a runtime split, it's compiletime.
is (qw(foo bar snorfle)[2], 'snorfle');

@ary = (12,23,34,45,56);

is(shift(@ary), 12);
is(pop(@ary), 56);
is(push(@ary,56), 4);
is(unshift(@ary,12), 5);

sub foo { "a" }
@foo=(foo())[0,0];
is ($foo[1], "a");

# $[ should have the same effect regardless of whether the aelem
#    op is optimized to aelemfast.



sub tary {
  local $[ = 10;
  my $five = 5;
  is ($tary[5], $tary[$five]);
}

@tary = (0..50);
tary();


# bugid #15439 - clearing an array calls destructors which may try
# to modify the array - caused 'Attempt to free unreferenced scalar'

my $got = runperl (
	prog => q{
		    sub X::DESTROY { @a = () }
		    @a = (bless {}, 'X');
		    @a = ();
		},
	stderr => 1
    );

$got =~ s/\n/ /g;
is ($got, '');

# Test negative and funky indices.


{
    my @a = 0..4;
    is($a[-1], 4);
    is($a[-2], 3);
    is($a[-5], 0);
    ok(!defined $a[-6]);

    is($a[2.1]  , 2);
    is($a[2.9]  , 2);
    is($a[undef], 0);
    is($a["3rd"], 3);
}


{
    my @a;
    eval '$a[-1] = 0';
    like($@, qr/Modification of non-creatable array value attempted, subscript -1/, "\$a[-1] = 0");
}

sub test_arylen {
    my $ref = shift;
    local $^W = 1;
    is ($$ref, undef, "\$# on freed array is undef");
    my @warn;
    local $SIG{__WARN__} = sub {push @warn, "@_"};
    $$ref = 1000;
    is (scalar @warn, 1);
    like ($warn[0], qr/^Attempt to set length of freed array/);
}

{
    my $a = \$#{[]};
    # Need a new statement to make it go out of scope
    test_arylen ($a);
    test_arylen (do {my @a; \$#a});
}

{
    use vars '@array';

    my $outer = \$#array;
    is ($$outer, -1);
    is (scalar @array, 0);

    $$outer = 3;
    is ($$outer, 3);
    is (scalar @array, 4);

    my $ref = \@array;

    my $inner;
    {
	local @array;
	$inner = \$#array;

	is ($$inner, -1);
	is (scalar @array, 0);
	$$outer = 6;

	is (scalar @$ref, 7);

	is ($$inner, -1);
	is (scalar @array, 0);

	$$inner = 42;
    }

    is (scalar @array, 7);
    is ($$outer, 6);

    is ($$inner, undef, "orphaned $#foo is always undef");

    is (scalar @array, 7);
    is ($$outer, 6);

    $$inner = 1;

    is (scalar @array, 7);
    is ($$outer, 6);

    $$inner = 503; # Bang!

    is (scalar @array, 7);
    is ($$outer, 6);
}

{
    # Bug #36211
    use vars '@array';
    for (1,2) {
	{
	    local @a;
	    is ($#a, -1);
	    @a=(1..4)
	}
    }
}

{
    # Bug #37350
    my @array = (1..4);
    $#{@array} = 7;
    is ($#{4}, 7);

    my $x;
    $#{$x} = 3;
    is(scalar @$x, 4);

    push @{@array}, 23;
    is ($4[8], 23);
}
{
    # Bug #37350 -- once more with a global
    use vars '@array';
    @array = (1..4);
    $#{@array} = 7;
    is ($#{4}, 7);

    my $x;
    $#{$x} = 3;
    is(scalar @$x, 4);

    push @{@array}, 23;
    is ($4[8], 23);
}

# more tests for AASSIGN_COMMON

{
    our($x,$y,$z) = (1..3);
    our($y,$z) = ($x,$y);
    is("$x $y $z", "1 1 2");
}
{
    our($x,$y,$z) = (1..3);
    (our $y, our $z) = ($x,$y);
    is("$x $y $z", "1 1 2");
}


"We're included by lib/Tie/Array/std.t so we need to return something true";
