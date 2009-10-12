#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib);
}

require 'test.pl';
use strict qw(refs subs);

plan(189);

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

sub PVBM () { 'foo' }
{ my $dummy = index 'foo', PVBM }

my $pviv = 1; "$pviv";
my $pvnv = 1.0; "$pvnv";
my $x;

# we don't test
#   tied lvalue => SCALAR, as we haven't tested tie yet
#   BIND, 'cos we can't create them yet
#   REGEXP, 'cos that requires overload or Scalar::Util
#   LVALUE ref, 'cos I can't work out how to create one :)

for (
    [ 'undef',          SCALAR  => \undef               ],
    [ 'constant IV',    SCALAR  => \1                   ],
    [ 'constant NV',    SCALAR  => \1.0                 ],
    [ 'constant PV',    SCALAR  => \'f'                 ],
    [ 'scalar',         SCALAR  => \$x                  ],
    [ 'PVIV',           SCALAR  => \$pviv               ],
    [ 'PVNV',           SCALAR  => \$pvnv               ],
    [ 'PVMG',           SCALAR  => \$0                  ],
    [ 'PVBM',           SCALAR  => \PVBM                ],
    [ 'vstring',        VSTRING => \v1                  ],
    [ 'ref',            REF     => \\1                  ],
    [ 'lvalue',         LVALUE  => \substr($x, 0, 0)    ],
    [ 'named array',    ARRAY   => \@ary                ],
    [ 'anon array',     ARRAY   => [ 1 ]                ],
    [ 'named hash',     HASH    => \%whatever           ],
    [ 'anon hash',      HASH    => { a => 1 }           ],
    [ 'named sub',      CODE    => \&mysub,             ],
    [ 'anon sub',       CODE    => sub { 1; }           ],
    [ 'glob',           GLOB    => \*foo                ],
    [ 'format',         FORMAT  => *STDERR{FORMAT}      ],
) {
    my ($desc, $type, $ref) = @$_;
    is (ref $ref, $type, "ref() for ref to $desc");
    like ("$ref", qr/^$type\(0x[0-9a-f]+\)$/, "stringify for ref to $desc");
}

is (ref *STDOUT{IO}, 'IO::Handle', 'IO refs are blessed into IO::Handle');
like (*STDOUT{IO}, qr/^IO::Handle=IO\(0x[0-9a-f]+\)$/,
    'stringify for IO refs');

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

TODO: {
    no strict 'refs';
    $name8 = chr 163;
    $name_utf8 = $name8 . chr 256;
    chop $name_utf8;

    is ($$name8, undef, 'Nothing before we start');
    is ($$name_utf8, undef, 'Nothing before we start');
    $$name8 = "Pound";
    is ($$name8, "Pound", 'Accessing via 8 bit symref works');
    local $TODO = "UTF8 mangled in symrefs";
    is ($$name_utf8, "Pound", 'Accessing via UTF8 symref works');
}

TODO: {
    no strict 'refs';
    $name_utf8 = $name = chr 9787;
    utf8::encode $name_utf8;

    is (length $name, 1, "Name is 1 char");
    is (length $name_utf8, 3, "UTF8 representation is 3 chars");

    is ($$name, undef, 'Nothing before we start');
    is ($$name_utf8, undef, 'Nothing before we start');
    $$name = "Face";
    is ($$name, "Face", 'Accessing via Unicode symref works');
    local $TODO = "UTF8 mangled in symrefs";
    is ($$name_utf8, undef,
	'Accessing via the UTF8 byte sequence gives nothing');
}

{
    no strict 'refs';
    $name1 = "\0Chalk";
    $name2 = "\0Cheese";

    isnt ($name1, $name2, "They differ");

    is ($$name1, undef, 'Nothing before we start (scalars)');
    is ($$name2, undef, 'Nothing before we start');
    $$name1 = "Yummy";
    is ($$name1, "Yummy", 'Accessing via the correct name works');
    is ($$name2, undef,
	'Accessing via a different NUL-containing name gives nothing');
    # defined uses a different code path
    ok (defined $$name1, 'defined via the correct name works');
    ok (!defined $$name2,
	'defined via a different NUL-containing name gives nothing');

    is ($name1->[0], undef, 'Nothing before we start (arrays)');
    is ($name2->[0], undef, 'Nothing before we start');
    $name1->[0] = "Yummy";
    is ($name1->[0], "Yummy", 'Accessing via the correct name works');
    is ($name2->[0], undef,
	'Accessing via a different NUL-containing name gives nothing');
    ok (defined $name1->[0], 'defined via the correct name works');
    ok (!defined$name2->[0],
	'defined via a different NUL-containing name gives nothing');

    my (undef, $one) = @{$name1}[2,3];
    my (undef, $two) = @{$name2}[2,3];
    is ($one, undef, 'Nothing before we start (array slices)');
    is ($two, undef, 'Nothing before we start');
    @{$name1}[2,3] = ("Very", "Yummy");
    (undef, $one) = @{$name1}[2,3];
    (undef, $two) = @{$name2}[2,3];
    is ($one, "Yummy", 'Accessing via the correct name works');
    is ($two, undef,
	'Accessing via a different NUL-containing name gives nothing');
    ok (defined $one, 'defined via the correct name works');
    ok (!defined $two,
	'defined via a different NUL-containing name gives nothing');

    is ($name1->{PWOF}, undef, 'Nothing before we start (hashes)');
    is ($name2->{PWOF}, undef, 'Nothing before we start');
    $name1->{PWOF} = "Yummy";
    is ($name1->{PWOF}, "Yummy", 'Accessing via the correct name works');
    is ($name2->{PWOF}, undef,
	'Accessing via a different NUL-containing name gives nothing');
    ok (defined $name1->{PWOF}, 'defined via the correct name works');
    ok (!defined $name2->{PWOF},
	'defined via a different NUL-containing name gives nothing');

    my (undef, $one) = @{$name1}{'SNIF', 'BEEYOOP'};
    my (undef, $two) = @{$name2}{'SNIF', 'BEEYOOP'};
    is ($one, undef, 'Nothing before we start (hash slices)');
    is ($two, undef, 'Nothing before we start');
    @{$name1}{'SNIF', 'BEEYOOP'} = ("Very", "Yummy");
    (undef, $one) = @{$name1}{'SNIF', 'BEEYOOP'};
    (undef, $two) = @{$name2}{'SNIF', 'BEEYOOP'};
    is ($one, "Yummy", 'Accessing via the correct name works');
    is ($two, undef,
	'Accessing via a different NUL-containing name gives nothing');
    ok (defined $one, 'defined via the correct name works');
    ok (!defined $two,
	'defined via a different NUL-containing name gives nothing');

    $name1 = "Left"; $name2 = "Left\0Right";
    my $glob2 = *{$name2};

    is ($glob1, undef, "We get different typeglobs. In fact, undef");

    *{$name1} = sub {"One"};
    *{$name2} = sub {"Two"};

    is (&{$name1}, "One");
    is (&{$name2}, "Two");
}

# test derefs after list slice

is ( ({foo => "bar"})[0]{foo}, "bar", 'hash deref from list slice w/o ->' );
is ( ({foo => "bar"})[0]->{foo}, "bar", 'hash deref from list slice w/ ->' );
is ( ([qw/foo bar/])[0][1], "bar", 'array deref from list slice w/o ->' );
is ( ([qw/foo bar/])[0]->[1], "bar", 'array deref from list slice w/ ->' );
is ( (sub {"bar"})[0](), "bar", 'code deref from list slice w/o ->' );
is ( (sub {"bar"})[0]->(), "bar", 'code deref from list slice w/ ->' );

# deref on empty list shouldn't autovivify
{
    local $@;
    eval { ()[0]{foo} };
    like ( "$@", "Can't use an undefined value as a HASH reference",
           "deref of undef from list slice fails" );
}

# test dereferencing errors
{
    format STDERR =
.
    my $ref;
    foreach $ref (*STDOUT{IO}, *STDERR{FORMAT}) {
	eval q/ $$ref /;
	like($@, qr/Not a SCALAR reference/, "Scalar dereference");
	eval q/ @$ref /;
	like($@, qr/Not an ARRAY reference/, "Array dereference");
	eval q/ %$ref /;
	like($@, qr/Not a HASH reference/, "Hash dereference");
	eval q/ &$ref /;
	like($@, qr/Not a CODE reference/, "Code dereference");
    }

    $ref = *STDERR{FORMAT};
    eval q/ *$ref /;
    like($@, qr/Not a GLOB reference/, "Glob dereference");

    $ref = *STDOUT{IO};
    eval q/ *$ref /;
    is($@, '', "Glob dereference of PVIO is acceptable");

    is($ref, *{$ref}{IO}, "IO slot of the temporary glob is set correctly");
}

# these will segfault if they fail

my $pvbm = PVBM;
my $rpvbm = \$pvbm;

ok (!eval { *$rpvbm }, 'PVBM ref is not a GLOB ref');
ok (!eval { *$pvbm }, 'PVBM is not a GLOB ref');
ok (!eval { $$pvbm }, 'PVBM is not a SCALAR ref');
ok (!eval { @$pvbm }, 'PVBM is not an ARRAY ref');
ok (!eval { %$pvbm }, 'PVBM is not a HASH ref');
ok (!eval { $pvbm->() }, 'PVBM is not a CODE ref');
ok (!eval { $rpvbm->foo }, 'PVBM is not an object');

# bug 24254
is( runperl(stderr => 1, prog => 'map eval qq(exit),1 for 1'), "");
is( runperl(stderr => 1, prog => 'eval { for (1) { map { die } 2 } };'), "");
is( runperl(stderr => 1, prog => 'for (125) { map { exit } (213)}'), "");
my $hushed = $^O eq 'VMS' ? 'use vmsish qw(hushed);' : '';
is( runperl(stderr => 1, prog => $hushed . 'map die,4 for 3'), "Died at -e line 1.\n");
is( runperl(stderr => 1, prog => $hushed . 'grep die,4 for 3'), "Died at -e line 1.\n");
is( runperl(stderr => 1, prog => $hushed . 'for $a (3) {@b=sort {die} 4,5}'), "Died at -e line 1.\n");

# bug 57564
is( runperl(stderr => 1, prog => 'my $i;for $i (1) { for $i (2) { } }'), "");


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

