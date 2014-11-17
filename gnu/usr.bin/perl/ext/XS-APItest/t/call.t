#!perl -w

# test the various call-into-perl-from-C functions
# DAPM Aug 2004

use warnings;
use strict;

# Test::More doesn't have fresh_perl_is() yet
# use Test::More tests => 342;

BEGIN {
    require '../../t/test.pl';
    plan(437);
    use_ok('XS::APItest')
};

#########################

# f(): general test sub to be called by call_sv() etc.
# Return the called args, but with the first arg replaced with 'b',
# and the last arg replaced with x/y/z depending on context
#
sub f {
    shift;
    unshift @_, 'b';
    pop @_;
    @_, defined wantarray ? wantarray ? 'x' :  'y' : 'z';
}

our $call_sv_count = 0;
sub i {
    $call_sv_count++;
}
call_sv_C();
is($call_sv_count, 6, "call_sv_C passes");

sub d {
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
	my $exp_warn = $keep ? "\t(in cleanup) its_dead_jim\n" : "";
	my $exp_err = $keep ? "before\n"
			    : "its_dead_jim\n";
	my $warn;
	local $SIG{__WARN__} = sub { $warn .= $_[0] };
	$@ = "before\n";
	$warn = "";
	ok(eq_array( [ call_sv('d', $flags|G_EVAL|$keep, @$args) ],
		    $returnval),
		    "$desc G_EVAL call_sv('d')");
	is($@, $exp_err, "$desc G_EVAL call_sv('d') - \$@");
	is($warn, $exp_warn, "$desc G_EVAL call_sv('d') - warning");

	$@ = "before\n";
	$warn = "";
	ok(eq_array( [ call_pv('d', $flags|G_EVAL|$keep, @$args) ], 
		    $returnval),
		    "$desc G_EVAL call_pv('d')");
	is($@, $exp_err, "$desc G_EVAL call_pv('d') - \$@");
	is($warn, $exp_warn, "$desc G_EVAL call_pv('d') - warning");

	$@ = "before\n";
	$warn = "";
	ok(eq_array( [ eval_sv('d()', $flags|$keep) ],
		    $returnval),
		    "$desc eval_sv('d()')");
	is($@, $exp_err, "$desc eval_sv('d()') - \$@");
	is($warn, $exp_warn, "$desc G_EVAL eval_sv('d') - warning");

	$@ = "before\n";
	$warn = "";
	ok(eq_array( [ call_method('d', $flags|G_EVAL|$keep, $obj, @$args) ],
		    $returnval),
		    "$desc G_EVAL call_method('d')");
	is($@, $exp_err, "$desc G_EVAL call_method('d') - \$@");
	is($warn, $exp_warn, "$desc G_EVAL call_method('d') - warning");
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

{
	# these are the ones documented in perlcall.pod
	my @flags = (G_DISCARD, G_NOARGS, G_EVAL, G_KEEPERR);
	my $mask = 0;
	$mask |= $_ for (@flags);
	is(unpack('%32b*', pack('l', $mask)), @flags,
	  "G_DISCARD and the rest are separate bits");
}

foreach my $inx ("", "aabbcc\n", [qw(aa bb cc)]) {
    foreach my $outx ("", "xxyyzz\n", [qw(xx yy zz)]) {
	my $warn;
	local $SIG{__WARN__} = sub { $warn .= $_[0] };
	$@ = $outx;
	$warn = "";
	call_sv(sub { die $inx if $inx }, G_VOID|G_EVAL);
	ok ref($@) eq ref($inx) && $@ eq $inx;
	$warn =~ s/ at [^\n]*\n\z//;
	is $warn, "";
	$@ = $outx;
	$warn = "";
	call_sv(sub { die $inx if $inx }, G_VOID|G_EVAL|G_KEEPERR);
	ok ref($@) eq ref($outx) && $@ eq $outx;
	$warn =~ s/ at [^\n]*\n\z//;
	is $warn, $inx ? "\t(in cleanup) $inx" : "";
    }
}

{
    no warnings "misc";
    my $warn = "";
    local $SIG{__WARN__} = sub { $warn .= $_[0] };
    call_sv(sub { die "aa\n" }, G_VOID|G_EVAL|G_KEEPERR);
    is $warn, "";
}

{
    no warnings "misc";
    my $warn = "";
    local $SIG{__WARN__} = sub { $warn .= $_[0] };
    call_sv(sub { use warnings "misc"; die "aa\n" }, G_VOID|G_EVAL|G_KEEPERR);
    is $warn, "\t(in cleanup) aa\n";
}

is(eval_pv('f()', 0), 'y', "eval_pv('f()', 0)");
is(eval_pv('f(qw(a b c))', 0), 'y', "eval_pv('f(qw(a b c))', 0)");
is(eval_pv('d()', 0), undef, "eval_pv('d()', 0)");
is($@, "its_dead_jim\n", "eval_pv('d()', 0) - \$@");
is(eval { eval_pv('d()', 1) } , undef, "eval { eval_pv('d()', 1) }");
is($@, "its_dead_jim\n", "eval { eval_pv('d()', 1) } - \$@");


# #3719 - check that the eval call variants handle exceptions correctly,
# and do the right thing with $@, both with and without G_KEEPERR set.

sub f99 { 99 };


for my $fn_type (0..2) { #   0:eval_pv   1:eval_sv   2:call_sv

    my $warn_msg;
    local $SIG{__WARN__} = sub { $warn_msg .= $_[0] };

    for my $code_type (0..3) {

	# call_sv can only handle function names, not code snippets
	next if $fn_type == 2 and ($code_type == 1 or $code_type == 2);

	my $code = (
	    'f99',			    # ok
	    '$x=',			    # compile-time err
	    'BEGIN { die "die in BEGIN"}',  # compile-time exception
	    'd', 			    # run-time exception
	)[$code_type];

	for my $keep (0, G_KEEPERR) {
	    my $keep_desc = $keep ? 'G_KEEPERR' : '0';

	    my $desc;
	    my $expect = ($code_type == 0) ? 1 : 0;

	    undef $warn_msg;
	    $@ = 'pre-err';

	    my @ret;
	    if ($fn_type == 0) { # eval_pv
		# eval_pv returns its result rather than a 'succeed' boolean
		$expect = $expect ? '99' : undef;

		# eval_pv doesn't support G_KEEPERR, but it has a croak
		# boolean arg instead, so switch on that instead
		if ($keep) {
		    $desc = "eval { eval_pv('$code', 1) }";
		    @ret = eval { eval_pv($code, 1); '99' };
		    # die in eval returns empty list
		    push @ret, undef unless @ret;
		}
		else {
		    $desc = "eval_pv('$code', 0)";
		    @ret = eval_pv($code, 0);
		}
	    }
	    elsif ($fn_type == 1) { # eval_sv
		$desc = "eval_sv('$code', G_ARRAY|$keep_desc)";
		@ret = eval_sv($code, G_ARRAY|$keep);
	    }
	    elsif ($fn_type == 2) { # call_sv
		$desc = "call_sv('$code', G_EVAL|G_ARRAY|$keep_desc)";
		@ret = call_sv($code, G_EVAL|G_ARRAY|$keep);
	    }
	    is(scalar @ret, ($code_type == 0 && $fn_type != 0) ? 2 : 1,
			    "$desc - number of returned args");
	    is($ret[-1], $expect, "$desc - return value");

	    if ($keep && $fn_type != 0) {
		# G_KEEPERR doesn't propagate into inner evals, requires etc
		unless ($keep && $code_type == 2) {
		    is($@, 'pre-err', "$desc - \$@ unmodified");
		}
		$@ = $warn_msg;
	    }
	    else {
		is($warn_msg, undef, "$desc - __WARN__ not called");
		unlike($@, 'pre-err', "$desc - \$@ modified");
	    }
	    like($@,
		(
		    qr/^$/,
		    qr/syntax error/,
		    qr/die in BEGIN/,
		    qr/its_dead_jim/,
		)[$code_type],
		"$desc - the correct error message");
	}
    }
}

# DAPM 9-Aug-04. A taint test in eval_sv() could die after setting up
# a new jump level but before pushing an eval context, leading to
# stack corruption

fresh_perl_is(<<'EOF', "x=2", { switches => ['-T', '-I../../lib'] }, 'eval_sv() taint');
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

