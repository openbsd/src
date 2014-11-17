#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = ('.', '../lib');
    require 'test.pl';
}

plan (171);

#
# @foo, @bar, and @ary are also used from tie-stdarray after tie-ing them
#

@ary = (1,2,3,4,5);
is(join('',@ary), '12345');

$tmp = $ary[$#ary]; --$#ary;
is($tmp, 5);
is($#ary, 3);
is(join('',@ary), '1234');

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
{
    no warnings 'deprecated';
    push(foo, 'blah');
}
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

# bugid #15439 - clearing an array calls destructors which may try
# to modify the array - caused 'Attempt to free unreferenced scalar'

my $got = runperl (
	prog => q{
		    sub X::DESTROY { @a = () }
		    @a = (bless {}, q{X});
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
{
    # AASSIGN_COMMON detection with logical operators
    my $true = 1;
    our($x,$y,$z) = (1..3);
    (our $y, our $z) = $true && ($x,$y);
    is("$x $y $z", "1 1 2");
}

# [perl #70171]
{
 my $x = get_x(); my %x = %$x; sub get_x { %x=(1..4); return \%x };
 is(
   join(" ", map +($_,$x{$_}), sort keys %x), "1 2 3 4",
  'bug 70171 (self-assignment via my %x = %$x)'
 );
 my $y = get_y(); my @y = @$y; sub get_y { @y=(1..4); return \@y };
 is(
  "@y", "1 2 3 4",
  'bug 70171 (self-assignment via my @x = @$x)'
 );
}

# [perl #70171], [perl #82110]
{
    my ($i, $ra, $rh);
  again:
    my @a = @$ra; # common assignment on 2nd attempt
    my %h = %$rh; # common assignment on 2nd attempt
    @a = qw(1 2 3 4);
    %h = qw(a 1 b 2 c 3 d 4);
    $ra = \@a;
    $rh = \%h;
    goto again unless $i++;

    is("@a", "1 2 3 4",
	'bug 70171 (self-assignment via my @x = @$x) - goto variant'
    );
    is(
	join(" ", map +($_,$h{$_}), sort keys %h), "a 1 b 2 c 3 d 4",
	'bug 70171 (self-assignment via my %x = %$x) - goto variant'
    );
}


*trit = *scile;  $trit[0];
ok(1, 'aelem_fast on a nonexistent array does not crash');

# [perl #107440]
sub A::DESTROY { $::ra = 0 }
$::ra = [ bless [], 'A' ];
undef @$::ra;
pass 'no crash when freeing array that is being undeffed';
$::ra = [ bless [], 'A' ];
@$::ra = ('a'..'z');
pass 'no crash when freeing array that is being cleared';

# [perl #85670] Copying magic to elements
SKIP: {
    skip "no Scalar::Util::weaken on miniperl", 1, if is_miniperl;
    require Scalar::Util;
    package glelp {
	Scalar::Util::weaken ($a = \@ISA);
	@ISA = qw(Foo);
	Scalar::Util::weaken ($a = \$ISA[0]);
	::is @ISA, 1, 'backref magic is not copied to elements';
    }
}
package peen {
    $#ISA = -1;
    @ISA = qw(Foo);
    $ISA[0] = qw(Sphare);

    sub Sphare::pling { 'pling' }

    ::is eval { pling peen }, 'pling',
	'arylen_p magic does not stop isa magic from being copied';
}

# Test that &PL_sv_undef is not special in arrays
sub {
    ok exists $_[0],
      'exists returns true for &PL_sv_undef elem [perl #7508]';
    is \$_[0], \undef, 'undef preserves identity in array [perl #109726]';
}->(undef);
# and that padav also knows how to handle the resulting NULLs
@_ = sub { my @a; $a[1]=1; @a }->();
is join (" ", map $_//"undef", @_), "undef 1",
  'returning my @a with nonexistent elements'; 

# [perl #118691]
@plink=@plunk=();
$plink[3] = 1;
sub {
    $_[0] = 2;
    is $plink[0], 2, '@_ alias to nonexistent elem within array';
    $_[1] = 3;
    is $plink[1], 3, '@_ alias to nonexistent neg index within array';
    is $_[2], undef, 'reading alias to negative index past beginning';
    eval { $_[2] = 42 };
    like $@, qr/Modification of non-creatable array value attempted, (?x:
               )subscript -5/,
         'error when setting alias to negative index past beginning';
    is $_[3], undef, 'reading alias to -1 elem of empty array';
    eval { $_[3] = 42 };
    like $@, qr/Modification of non-creatable array value attempted, (?x:
               )subscript -1/,
         'error when setting alias to -1 elem of empty array';
}->($plink[0], $plink[-2], $plink[-5], $plunk[-1]);

$_ = \$#{[]};
$$_ = \1;
"$$_";
pass "no assertion failure after assigning ref to arylen when ary is gone";


{
    # Test aelemfast for both +ve and -ve indices, both lex and package vars.
    # Make especially careful that we don't have any edge cases around
    # fitting an I8 into a U8.
    my @a = (0..299);
    is($a[-256], 300-256, 'lex -256');
    is($a[-255], 300-255, 'lex -255');
    is($a[-254], 300-254, 'lex -254');
    is($a[-129], 300-129, 'lex -129');
    is($a[-128], 300-128, 'lex -128');
    is($a[-127], 300-127, 'lex -127');
    is($a[-126], 300-126, 'lex -126');
    is($a[  -1], 300-  1, 'lex   -1');
    is($a[   0],       0, 'lex    0');
    is($a[   1],       1, 'lex    1');
    is($a[ 126],     126, 'lex  126');
    is($a[ 127],     127, 'lex  127');
    is($a[ 128],     128, 'lex  128');
    is($a[ 129],     129, 'lex  129');
    is($a[ 254],     254, 'lex  254');
    is($a[ 255],     255, 'lex  255');
    is($a[ 256],     256, 'lex  256');
    @aelem =(0..299);
    is($aelem[-256], 300-256, 'pkg -256');
    is($aelem[-255], 300-255, 'pkg -255');
    is($aelem[-254], 300-254, 'pkg -254');
    is($aelem[-129], 300-129, 'pkg -129');
    is($aelem[-128], 300-128, 'pkg -128');
    is($aelem[-127], 300-127, 'pkg -127');
    is($aelem[-126], 300-126, 'pkg -126');
    is($aelem[  -1], 300-  1, 'pkg   -1');
    is($aelem[   0],       0, 'pkg    0');
    is($aelem[   1],       1, 'pkg    1');
    is($aelem[ 126],     126, 'pkg  126');
    is($aelem[ 127],     127, 'pkg  127');
    is($aelem[ 128],     128, 'pkg  128');
    is($aelem[ 129],     129, 'pkg  129');
    is($aelem[ 254],     254, 'pkg  254');
    is($aelem[ 255],     255, 'pkg  255');
    is($aelem[ 256],     256, 'pkg  256');
}

"We're included by lib/Tie/Array/std.t so we need to return something true";
