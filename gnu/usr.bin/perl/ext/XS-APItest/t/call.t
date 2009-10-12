#!perl -w

# test the various call-into-perl-from-C functions
# DAPM Aug 2004

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    push @INC, "::lib:$MacPerl::Architecture:" if $^O eq 'MacOS';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bXS\/APItest\b/) {
	# Look, I'm using this fully-qualified variable more than once!
	my $arch = $MacPerl::Architecture;
        print "1..0 # Skip: XS::APItest was not built\n";
        exit 0;
    }
}

use warnings;
use strict;

# Test::More doesn't have fresh_perl_is() yet
# use Test::More tests => 240;

BEGIN {
    require './test.pl';
    plan(240);
    use_ok('XS::APItest')
};

#########################

sub f {
    shift;
    unshift @_, 'b';
    pop @_;
    @_, defined wantarray ? wantarray ? 'x' :  'y' : 'z';
}

sub d {
    no warnings 'misc'; # keep G_KEEPERR from emitting this as a warning
    die "its_dead_jim\n";
}

my $obj = bless [], 'Foo';

sub Foo::meth {
    return 'bad_self' unless @_ && ref $_[0] && ref($_[0]) eq 'Foo';
    shift;
    shift;
    unshift @_, 'b';
    pop @_;
    @_, defined wantarray ? wantarray ? 'x' :  'y' : 'z';
}

sub Foo::d {
    no warnings 'misc'; # keep G_KEEPERR from emitting this as a warning
    die "its_dead_jim\n";
}

for my $test (
    # flags      args           expected         description
    [ G_VOID,    [ ],           [ qw(z 1) ],     '0 args, G_VOID' ],
    [ G_VOID,    [ qw(a p q) ], [ qw(z 1) ],     '3 args, G_VOID' ],
    [ G_SCALAR,  [ ],           [ qw(y 1) ],     '0 args, G_SCALAR' ],
    [ G_SCALAR,  [ qw(a p q) ], [ qw(y 1) ],     '3 args, G_SCALAR' ],
    [ G_ARRAY,   [ ],           [ qw(x 1) ],     '0 args, G_ARRAY' ],
    [ G_ARRAY,   [ qw(a p q) ], [ qw(b p x 3) ], '3 args, G_ARRAY' ],
    [ G_DISCARD, [ ],           [ qw(0) ],       '0 args, G_DISCARD' ],
    [ G_DISCARD, [ qw(a p q) ], [ qw(0) ],       '3 args, G_DISCARD' ],
)
{
    my ($flags, $args, $expected, $description) = @$test;

    ok(eq_array( [ call_sv(\&f, $flags, @$args) ], $expected),
	"$description call_sv(\\&f)");

    ok(eq_array( [ call_sv(*f,  $flags, @$args) ], $expected),
	"$description call_sv(*f)");

    ok(eq_array( [ call_sv('f', $flags, @$args) ], $expected),
	"$description call_sv('f')");

    ok(eq_array( [ call_pv('f', $flags, @$args) ], $expected),
	"$description call_pv('f')");

    ok(eq_array( [ eval_sv('f(' . join(',',map"'$_'",@$args) . ')', $flags) ],
	$expected), "$description eval_sv('f(args)')");

    ok(eq_array( [ call_method('meth', $flags, $obj, @$args) ], $expected),
	"$description call_method('meth')");

    my $returnval = ((($flags & G_WANT) == G_ARRAY) || ($flags & G_DISCARD))
	? [0] : [ undef, 1 ];
    for my $keep (0, G_KEEPERR) {
	my $desc = $description . ($keep ? ' G_KEEPERR' : '');
	my $exp_err = $keep ? "before\n\t(in cleanup) its_dead_jim\n"
			    : "its_dead_jim\n";
	$@ = "before\n";
	ok(eq_array( [ call_sv('d', $flags|G_EVAL|$keep, @$args) ],
		    $returnval),
		    "$desc G_EVAL call_sv('d')");
	is($@, $exp_err, "$desc G_EVAL call_sv('d') - \$@");

	$@ = "before\n";
	ok(eq_array( [ call_pv('d', $flags|G_EVAL|$keep, @$args) ], 
		    $returnval),
		    "$desc G_EVAL call_pv('d')");
	is($@, $exp_err, "$desc G_EVAL call_pv('d') - \$@");

	$@ = "before\n";
	ok(eq_array( [ eval_sv('d()', $flags|$keep) ],
		    $returnval),
		    "$desc eval_sv('d()')");
	is($@, $exp_err, "$desc eval_sv('d()') - \$@");

	$@ = "before\n";
	ok(eq_array( [ call_method('d', $flags|G_EVAL|$keep, $obj, @$args) ],
		    $returnval),
		    "$desc G_EVAL call_method('d')");
	is($@, $exp_err, "$desc G_EVAL call_method('d') - \$@");
    }

    ok(eq_array( [ sub { call_sv('f', $flags|G_NOARGS, "bad") }->(@$args) ],
	$expected), "$description G_NOARGS call_sv('f')");

    ok(eq_array( [ sub { call_pv('f', $flags|G_NOARGS, "bad") }->(@$args) ],
	$expected), "$description G_NOARGS call_pv('f')");

    ok(eq_array( [ sub { eval_sv('f(@_)', $flags|G_NOARGS) }->(@$args) ],
	$expected), "$description G_NOARGS eval_sv('f(@_)')");

    # XXX call_method(G_NOARGS) isn't tested: I'm assuming
    # it's not a sensible combination. DAPM.

    ok(eq_array( [ eval { call_sv('d', $flags, @$args)}, $@ ],
	[ "its_dead_jim\n" ]), "$description eval { call_sv('d') }");

    ok(eq_array( [ eval { call_pv('d', $flags, @$args) }, $@ ],
	[ "its_dead_jim\n" ]), "$description eval { call_pv('d') }");

    ok(eq_array( [ eval { eval_sv('d', $flags), $@ }, $@ ],
	[ @$returnval,
		"its_dead_jim\n", '' ]),
	"$description eval { eval_sv('d') }");

    ok(eq_array( [ eval { call_method('d', $flags, $obj, @$args) }, $@ ],
	[ "its_dead_jim\n" ]), "$description eval { call_method('d') }");

};

is(eval_pv('f()', 0), 'y', "eval_pv('f()', 0)");
is(eval_pv('f(qw(a b c))', 0), 'y', "eval_pv('f(qw(a b c))', 0)");
is(eval_pv('d()', 0), undef, "eval_pv('d()', 0)");
is($@, "its_dead_jim\n", "eval_pv('d()', 0) - \$@");
is(eval { eval_pv('d()', 1) } , undef, "eval { eval_pv('d()', 1) }");
is($@, "its_dead_jim\n", "eval { eval_pv('d()', 1) } - \$@");

# DAPM 9-Aug-04. A taint test in eval_sv() could die after setting up
# a new jump level but before pushing an eval context, leading to
# stack corruption

fresh_perl_is(<<'EOF', "x=2", { switches => ['-T'] }, 'eval_sv() taint');
use XS::APItest;

my $x = 0;
sub f {
    eval { my @a = ($^X . "x" , eval_sv(q(die "inner\n"), 0)) ; };
    $x++;
    $a <=> $b;
}

eval { my @a = sort f 2, 1;  $x++};
print "x=$x\n";
EOF

