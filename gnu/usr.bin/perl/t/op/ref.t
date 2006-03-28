#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib);
}

require 'test.pl';
use strict qw(refs subs);

plan (74);

# Test glob operations.

$bar = "one";
$foo = "two";
{
    local(*foo) = *bar;
    is($foo, 'one');
}
is ($foo, 'two');

$baz = "three";
$foo = "four";
{
    local(*foo) = 'baz';
    is ($foo, 'three');
}
is ($foo, 'four');

$foo = "global";
{
    local(*foo);
    is ($foo, undef);
    $foo = "local";
    is ($foo, 'local');
}
is ($foo, 'global');

{
    no strict 'refs';
# Test fake references.

    $baz = "valid";
    $bar = 'baz';
    $foo = 'bar';
    is ($$$foo, 'valid');
}

# Test real references.

$FOO = \$BAR;
$BAR = \$BAZ;
$BAZ = "hit";
is ($$$FOO, 'hit');

# Test references to real arrays.

my $test = curr_test();
@ary = ($test,$test+1,$test+2,$test+3);
$ref[0] = \@a;
$ref[1] = \@b;
$ref[2] = \@c;
$ref[3] = \@d;
for $i (3,1,2,0) {
    push(@{$ref[$i]}, "ok $ary[$i]\n");
}
print @a;
print ${$ref[1]}[0];
print @{$ref[2]}[0];
{
    no strict 'refs';
    print @{'d'};
}
curr_test($test+4);

# Test references to references.

$refref = \\$x;
$x = "Good";
is ($$$refref, 'Good');

# Test nested anonymous lists.

$ref = [[],2,[3,4,5,]];
is (scalar @$ref, 3);
is ($$ref[1], 2);
is (${$$ref[2]}[2], 5);
is (scalar @{$$ref[0]}, 0);

is ($ref->[1], 2);
is ($ref->[2]->[0], 3);

# Test references to hashes of references.

$refref = \%whatever;
$refref->{"key"} = $ref;
is ($refref->{"key"}->[2]->[0], 3);

# Test to see if anonymous subarrays spring into existence.

$spring[5]->[0] = 123;
$spring[5]->[1] = 456;
push(@{$spring[5]}, 789);
is (join(':',@{$spring[5]}), "123:456:789");

# Test to see if anonymous subhashes spring into existence.

@{$spring2{"foo"}} = (1,2,3);
$spring2{"foo"}->[3] = 4;
is (join(':',@{$spring2{"foo"}}), "1:2:3:4");

# Test references to subroutines.

{
    my $called;
    sub mysub { $called++; }
    $subref = \&mysub;
    &$subref;
    is ($called, 1);
}

$subrefref = \\&mysub2;
is ($$subrefref->("GOOD"), "good");
sub mysub2 { lc shift }

# Test the ref operator.

is (ref $subref, 'CODE');
is (ref $ref, 'ARRAY');
is (ref $refref, 'HASH');

# Test anonymous hash syntax.

$anonhash = {};
is (ref $anonhash, 'HASH');
$anonhash2 = {FOO => 'BAR', ABC => 'XYZ',};
is (join('', sort values %$anonhash2), 'BARXYZ');

# Test bless operator.

package MYHASH;

$object = bless $main'anonhash2;
main::is (ref $object, 'MYHASH');
main::is ($object->{ABC}, 'XYZ');

$object2 = bless {};
main::is (ref $object2,	'MYHASH');

# Test ordinary call on object method.

&mymethod($object,"argument");

sub mymethod {
    local($THIS, @ARGS) = @_;
    die 'Got a "' . ref($THIS). '" instead of a MYHASH'
	unless ref $THIS eq 'MYHASH';
    main::is ($ARGS[0], "argument");
    main::is ($THIS->{FOO}, 'BAR');
}

# Test automatic destructor call.

$string = "bad";
$object = "foo";
$string = "good";
$main'anonhash2 = "foo";
$string = "";

DESTROY {
    return unless $string;
    main::is ($string, 'good');

    # Test that the object has not already been "cursed".
    main::isnt (ref shift, 'HASH');
}

# Now test inheritance of methods.

package OBJ;

@ISA = ('BASEOBJ');

$main'object = bless {FOO => 'foo', BAR => 'bar'};

package main;

# Test arrow-style method invocation.

is ($object->doit("BAR"), 'bar');

# Test indirect-object-style method invocation.

$foo = doit $object "FOO";
main::is ($foo, 'foo');

sub BASEOBJ'doit {
    local $ref = shift;
    die "Not an OBJ" unless ref $ref eq 'OBJ';
    $ref->{shift()};
}

package UNIVERSAL;
@ISA = 'LASTCHANCE';

package LASTCHANCE;
sub foo { main::is ($_[1], 'works') }

package WHATEVER;
foo WHATEVER "works";

#
# test the \(@foo) construct
#
package main;
@foo = \(1..3);
@bar = \(@foo);
@baz = \(1,@foo,@bar);
is (scalar (@bar), 3);
is (scalar grep(ref($_), @bar), 3);
is (scalar (@baz), 3);

my(@fuu) = \(1..2,3);
my(@baa) = \(@fuu);
my(@bzz) = \(1,@fuu,@baa);
is (scalar (@baa), 3);
is (scalar grep(ref($_), @baa), 3);
is (scalar (@bzz), 3);

# also, it can't be an lvalue
eval '\\($x, $y) = (1, 2);';
like ($@, qr/Can\'t modify.*ref.*in.*assignment/);

# test for proper destruction of lexical objects
$test = curr_test();
sub larry::DESTROY { print "# larry\nok $test\n"; }
sub curly::DESTROY { print "# curly\nok ", $test + 1, "\n"; }
sub moe::DESTROY   { print "# moe\nok ", $test + 2, "\n"; }

{
    my ($joe, @curly, %larry);
    my $moe = bless \$joe, 'moe';
    my $curly = bless \@curly, 'curly';
    my $larry = bless \%larry, 'larry';
    print "# leaving block\n";
}

print "# left block\n";
curr_test($test + 3);

# another glob test


$foo = "garbage";
{ local(*bar) = "foo" }
$bar = "glob 3";
local(*bar) = *bar;
is ($bar, "glob 3");

$var = "glob 4";
$_   = \$var;
is ($$_, 'glob 4');


# test if reblessing during destruction results in more destruction
$test = curr_test();
{
    package A;
    sub new { bless {}, shift }
    DESTROY { print "# destroying 'A'\nok ", $test + 1, "\n" }
    package _B;
    sub new { bless {}, shift }
    DESTROY { print "# destroying '_B'\nok $test\n"; bless shift, 'A' }
    package main;
    my $b = _B->new;
}
curr_test($test + 2);

# test if $_[0] is properly protected in DESTROY()

{
    my $test = curr_test();
    my $i = 0;
    local $SIG{'__DIE__'} = sub {
	my $m = shift;
	if ($i++ > 4) {
	    print "# infinite recursion, bailing\nnot ok $test\n";
	    exit 1;
        }
	like ($m, qr/^Modification of a read-only/);
    };
    package C;
    sub new { bless {}, shift }
    DESTROY { $_[0] = 'foo' }
    {
	print "# should generate an error...\n";
	my $c = C->new;
    }
    print "# good, didn't recurse\n";
}

# test if refgen behaves with autoviv magic
{
    my @a;
    $a[1] = "good";
    my $got;
    for (@a) {
	$got .= ${\$_};
	$got .= ';';
    }
    is ($got, ";good;");
}

# This test is the reason for postponed destruction in sv_unref
$a = [1,2,3];
$a = $a->[1];
is ($a, 2);

# This test used to coredump. The BEGIN block is important as it causes the
# op that created the constant reference to be freed. Hence the only
# reference to the constant string "pass" is in $a. The hack that made
# sure $a = $a->[1] would work didn't work with references to constants.


foreach my $lexical ('', 'my $a; ') {
  my $expect = "pass\n";
  my $result = runperl (switches => ['-wl'], stderr => 1,
    prog => $lexical . 'BEGIN {$a = \q{pass}}; $a = $$a; print $a');

  is ($?, 0);
  is ($result, $expect);
}

$test = curr_test();
sub x::DESTROY {print "ok ", $test + shift->[0], "\n"}
{ my $a1 = bless [3],"x";
  my $a2 = bless [2],"x";
  { my $a3 = bless [1],"x";
    my $a4 = bless [0],"x";
    567;
  }
}
curr_test($test+4);

is (runperl (switches=>['-l'],
	     prog=> 'print 1; print qq-*$\*-;print 1;'),
    "1\n*\n*\n1\n");

# bug #21347

runperl(prog => 'sub UNIVERSAL::AUTOLOAD { qr// } a->p' );
is ($?, 0, 'UNIVERSAL::AUTOLOAD called when freeing qr//');

runperl(prog => 'sub UNIVERSAL::DESTROY { warn } bless \$a, A', stderr => 1);
is ($?, 0, 'warn called inside UNIVERSAL::DESTROY');


# bug #22719

runperl(prog => 'sub f { my $x = shift; *z = $x; } f({}); f();');
is ($?, 0, 'coredump on typeglob = (SvRV && !SvROK)');

# bug #27268: freeing self-referential typeglobs could trigger
# "Attempt to free unreferenced scalar" warnings

is (runperl(
    prog => 'use Symbol;my $x=bless \gensym,"t"; print;*$$x=$x',
    stderr => 1
), '', 'freeing self-referential typeglob');

# using a regex in the destructor for STDOUT segfaulted because the
# REGEX pad had already been freed (ithreads build only). The
# object is required to trigger the early freeing of GV refs to to STDOUT

like (runperl(
    prog => '$x=bless[]; sub IO::Handle::DESTROY{$_="bad";s/bad/ok/;print}',
    stderr => 1
      ), qr/^(ok)+$/, 'STDOUT destructor');

# Bit of a hack to make test.pl happy. There are 3 more tests after it leaves.
$test = curr_test();
curr_test($test + 3);
# test global destruction

my $test1 = $test + 1;
my $test2 = $test + 2;

package FINALE;

{
    $ref3 = bless ["ok $test2\n"];	# package destruction
    my $ref2 = bless ["ok $test1\n"];	# lexical destruction
    local $ref1 = bless ["ok $test\n"];	# dynamic destruction
    1;					# flush any temp values on stack
}

DESTROY {
    print $_[0][0];
}

