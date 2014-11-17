#!./perl

BEGIN {
    unshift @INC, 't';
    require Config;
    if (($Config::Config{'extensions'} !~ /\bB\b/) ){
        print "1..0 # Skip -- Perl configured without B module\n";
        exit 0;
    }
}

$|  = 1;
use warnings;
use strict;
use Test::More;

BEGIN { use_ok( 'B' ); }


package Testing::Symtable;
use vars qw($This @That %wibble $moo %moo);
my $not_a_sym = 'moo';

sub moo { 42 }
sub car { 23 }


package Testing::Symtable::Foo;
sub yarrow { "Hock" }

package Testing::Symtable::Bar;
sub hock { "yarrow" }

package main;
use vars qw(%Subs);
local %Subs = ();
B::walksymtable(\%Testing::Symtable::, 'find_syms', sub { $_[0] =~ /Foo/ },
                'Testing::Symtable::');

sub B::GV::find_syms {
    my($symbol) = @_;

    $main::Subs{$symbol->STASH->NAME . '::' . $symbol->NAME}++;
}

my @syms = map { 'Testing::Symtable::'.$_ } qw(This That wibble moo car
                                               BEGIN);
push @syms, "Testing::Symtable::Foo::yarrow";

# Make sure we hit all the expected symbols.
ok( join('', sort @syms) eq join('', sort keys %Subs), 'all symbols found' );

# Make sure we only hit them each once.
ok( (!grep $_ != 1, values %Subs), '...and found once' );

# Tests for MAGIC / MOREMAGIC
ok( B::svref_2object(\$.)->MAGIC->TYPE eq "\0", '$. has \0 magic' );
{
    my $e = '';
    local $SIG{__DIE__} = sub { $e = $_[0] };
    # Used to dump core, bug #16828
    eval { B::svref_2object(\$.)->MAGIC->MOREMAGIC->TYPE; };
    like( $e, qr/Can't call method "TYPE" on an undefined value/, 
	'$. has no more magic' );
}

{
    my $pie = 'Good';
    # This needs to be a package variable, as vars in the pad have some flags.
    my $r = B::svref_2object(\$::data2);
    is($r->FLAGS(), 0, "uninitialised package variable has flags of 0");
    is($r->SvTYPE(), 0, "uninitialised package variable has type 0");
    is($r->POK(), 0, "POK false");
    is($r->ROK(), 0, "ROK false");
    is($r->MAGICAL(), 0, "MAGICAL false");
    $::data2 = $pie;
    isnt($r->FLAGS(), 0, "initialised package variable has nonzero flags");
    isnt($r->SvTYPE(), 0, "initialised package variable has nonzero type");
    isnt($r->POK(), 0, "POK true");
    is($r->ROK(), 0, "ROK false");
    is($r->MAGICAL(), 0, "MAGICAL false");

    $::data2 = substr $pie, 0, 1;
    isnt($r->FLAGS(), 0, "initialised package variable has nonzero flags");
    isnt($r->SvTYPE(), 0, "initialised package variable has nonzero type");
    isnt($r->POK(), 0, "POK true");
    is($r->ROK(), 0, "ROK false");
    is($r->MAGICAL(), 0, "MAGICAL true");

    $::data2 = \$pie;
    isnt($r->FLAGS(), 0, "initialised package variable has nonzero flags");
    isnt($r->SvTYPE(), 0, "initialised package variable has nonzero type");
    is($r->POK(), 0, "POK false");
    isnt($r->ROK(), 0, "ROK true");
    is($r->MAGICAL(), 0, "MAGICAL false");

    is($r->REFCNT(), 1, "Reference count is 1");
    {
	my $ref = \$::data2;
	is($r->REFCNT(), 2, "Second reference");
    }
    is($r->REFCNT(), 1, "Reference count is 1");

}

my $r = qr/foo/;
my $obj = B::svref_2object($r);
my $regexp =  ($] < 5.011) ? $obj->MAGIC : $obj;
ok($regexp->precomp() eq 'foo', 'Get string from qr//');
like($regexp->REGEX(), qr/\d+/, "REGEX() returns numeric value");
my $iv = 1;
my $iv_ref = B::svref_2object(\$iv);
is(ref $iv_ref, "B::IV", "Test B:IV return from svref_2object");
is($iv_ref->REFCNT, 1, "Test B::IV->REFCNT");
# Flag tests are needed still
#diag $iv_ref->FLAGS();
my $iv_ret = $iv_ref->object_2svref();
is(ref $iv_ret, "SCALAR", "Test object_2svref() return is SCALAR");
is($$iv_ret, $iv, "Test object_2svref()");
is($iv_ref->int_value, $iv, "Test int_value()");
is($iv_ref->IV, $iv, "Test IV()");
is($iv_ref->IVX(), $iv, "Test IVX()");
is($iv_ref->UVX(), $iv, "Test UVX()");
is(eval { $iv_ref->RV() }, undef, 'Test RV() on IV');
like($@, qr/argument is not SvROK/, 'Test RV() IV');
$iv = \"Pie";
my $val = eval { $iv_ref->RV() };
is(ref $val, 'B::PV', 'Test RV() on a reference');
is($val->PV(), 'Pie', 'Value expected');
is($@, '', "Test RV()");

my $pv = "Foo";
my $pv_ref = B::svref_2object(\$pv);
is(ref $pv_ref, "B::PV", "Test B::PV return from svref_2object");
is($pv_ref->REFCNT, 1, "Test B::PV->REFCNT");
# Flag tests are needed still
#diag $pv_ref->FLAGS();
my $pv_ret = $pv_ref->object_2svref();
is(ref $pv_ret, "SCALAR", "Test object_2svref() return is SCALAR");
is($$pv_ret, $pv, "Test object_2svref()");
is($pv_ref->PV(), $pv, "Test PV()");
is(eval { $pv_ref->RV() }, undef, 'Test RV() on PV');
like($@, qr/argument is not SvROK/, 'Test RV() on PV');
is($pv_ref->PVX(), $pv, "Test PVX()");
$pv = \"Pie";
$val = eval { $pv_ref->RV() };
is(ref $val, 'B::PV', 'Test RV() on a reference');
is($val->PV(), 'Pie', 'Value expected');
is($@, '', "Test RV()");

my $nv = 1.1;
my $nv_ref = B::svref_2object(\$nv);
is(ref $nv_ref, "B::NV", "Test B::NV return from svref_2object");
is($nv_ref->REFCNT, 1, "Test B::NV->REFCNT");
# Flag tests are needed still
#diag $nv_ref->FLAGS();
my $nv_ret = $nv_ref->object_2svref();
is(ref $nv_ret, "SCALAR", "Test object_2svref() return is SCALAR");
is($$nv_ret, $nv, "Test object_2svref()");
is($nv_ref->NV, $nv, "Test NV()");
is($nv_ref->NVX(), $nv, "Test NVX()");
is(eval { $nv_ref->RV() }, undef, 'Test RV() on NV');
like($@, qr/Can't locate object method "RV" via package "B::NV"/,
     'Test RV() on NV');

my $null = undef;
my $null_ref = B::svref_2object(\$null);
is(ref $null_ref, "B::NULL", "Test B::NULL return from svref_2object");
is($null_ref->REFCNT, 1, "Test B::NULL->REFCNT");
# Flag tests are needed still
#diag $null_ref->FLAGS();
my $null_ret = $nv_ref->object_2svref();
is(ref $null_ret, "SCALAR", "Test object_2svref() return is SCALAR");
is($$null_ret, $nv, "Test object_2svref()");

my $RV_class = $] >= 5.011 ? 'B::IV' : 'B::RV';
my $cv = sub{ 1; };
my $cv_ref = B::svref_2object(\$cv);
is($cv_ref->REFCNT, 1, "Test $RV_class->REFCNT");
is(ref $cv_ref, "$RV_class",
   "Test $RV_class return from svref_2object - code");
my $cv_ret = $cv_ref->object_2svref();
is(ref $cv_ret, "REF", "Test object_2svref() return is REF");
is($$cv_ret, $cv, "Test object_2svref()");

my $av = [];
my $av_ref = B::svref_2object(\$av);
is(ref $av_ref, "$RV_class",
   "Test $RV_class return from svref_2object - array");

my $hv = [];
my $hv_ref = B::svref_2object(\$hv);
is(ref $hv_ref, "$RV_class",
   "Test $RV_class return from svref_2object - hash");

local *gv = *STDOUT;
my $gv_ref = B::svref_2object(\*gv);
is(ref $gv_ref, "B::GV", "Test B::GV return from svref_2object");
ok(! $gv_ref->is_empty(), "Test is_empty()");
ok($gv_ref->isGV_with_GP(), "Test isGV_with_GP()");
is($gv_ref->NAME(), "gv", "Test NAME()");
is($gv_ref->SAFENAME(), "gv", "Test SAFENAME()");
like($gv_ref->FILE(), qr/b\.t$/, "Testing FILE()");
is($gv_ref->SvTYPE(), B::SVt_PVGV, "Test SvTYPE()");
is($gv_ref->FLAGS() & B::SVTYPEMASK, B::SVt_PVGV, "Test SVTYPEMASK");

# The following return B::SPECIALs.
is(ref B::sv_yes(), "B::SPECIAL", "B::sv_yes()");
is(ref B::sv_no(), "B::SPECIAL", "B::sv_no()");
is(ref B::sv_undef(), "B::SPECIAL", "B::sv_undef()");

# More utility functions
is(B::ppname(0), "pp_null", "Testing ppname (this might break if opnames.h is changed)");
is(B::opnumber("null"), 0, "Testing opnumber with opname (null)");
is(B::opnumber("pp_null"), 0, "Testing opnumber with opname (pp_null)");
{
    my $hash = B::hash("wibble");
    like($hash, qr/\A0x[0-9a-f]+\z/, "Testing B::hash(\"wibble\")");
    unlike($hash, qr/\A0x0+\z/, "Testing B::hash(\"wibble\")");

    SKIP: {
        skip "Nulls don't hash to the same bucket regardless of length with this PERL_HASH implementation", 20
            if B::hash("") ne B::hash("\0" x 19);
        like(B::hash("\0" x $_), qr/\A0x0+\z/, "Testing B::hash(\"0\" x $_)")
             for 0..19;
    }

    $hash = eval {B::hash(chr 256)};
    is($hash, undef, "B::hash() refuses non-octets");
    like($@, qr/^Wide character in subroutine entry/);

    $hash = B::hash(chr 163);
    my $str = chr(163) . chr 256;
    chop $str;
    is(B::hash($str), $hash, 'B::hash() with chr 128-256 is well-behaved');
}
{
    is(B::cstring(undef), '0', "Testing B::cstring(undef)");
    is(B::perlstring(undef), '0', "Testing B::perlstring(undef)");

    my @common = map {eval $_, $_}
	'"wibble"', '"\""', '"\'"', '"\\\\"', '"\\n\\r\\t\\b\\a\\f"', '"\000"',
	    '"\000\000"', '"\000Bing\000"', ord 'N' == 78 ? '"\\177"' : ();

    my $oct = sprintf "\\%03o", ord '?';
    my @tests = (@common, '$_', '"$_"', '@_', '"@_"', '??N', qq{"$oct?N"},
		 ord 'N' == 78 ? (chr 11, '"\v"'): ());
    while (my ($test, $expect) = splice @tests, 0, 2) {
	is(B::cstring($test), $expect, "B::cstring($expect)");
    }

    @tests = (@common, '$_', '"\$_"', '@_', '"\@_"', '??N', '"??N"',
	      chr 256, '"\x{100}"', chr 65536, '"\x{10000}"',
	      ord 'N' == 78 ? (chr 11, '"\013"'): ());
    while (my ($test, $expect) = splice @tests, 0, 2) {
	is(B::perlstring($test), $expect, "B::perlstring($expect)");
	utf8::upgrade $test;
	$expect =~ s/\\b/\\x\{8\}/g;
	$expect =~ s/\\([0-7]{3})/sprintf "\\x\{%x\}", oct $1/eg;
	is(B::perlstring($test), $expect, "B::perlstring($expect) (Unicode)");
    }
}
{
    my @tests = ((map {eval(qq{"$_"}), $_} '\\n', '\\r', '\\t',
		  '\\b', '\\a', '\\f', '\\000', '\\\'', '?'), '"', '"',
		 ord 'N' == 78 ? (chr 11, '\v', "\177", '\\177') : ());

    while (my ($test, $expect) = splice @tests, 0, 2) {
	is(B::cchar($test), "'${expect}'", "B::cchar(qq{$expect})");
    }
}

is(B::class(bless {}, "Wibble::Bibble"), "Bibble", "Testing B::class()");
is(B::cast_I32(3.14), 3, "Testing B::cast_I32()");
is(B::opnumber("chop"), $] >= 5.015 ? 39 : 38,
			    "Testing opnumber with opname (chop)");

{
    no warnings 'once';
    my $sg = B::sub_generation();
    *UNIVERSAL::hand_waving = sub { };
    ok( $sg < B::sub_generation, "sub_generation increments" );
}

like( B::amagic_generation, qr/^\d+\z/, "amagic_generation" );

is(B::svref_2object(sub {})->ROOT->ppaddr, 'PL_ppaddr[OP_LEAVESUB]',
   'OP->ppaddr');

# This one crashes from perl 5.8.9 to B 1.24 (perl 5.13.6):
B::svref_2object(sub{y/\x{100}//})->ROOT->first->first->sibling->sv;
ok 1, 'B knows that UTF trans is a padop in 5.8.9, not an svop';

{
    format FOO =
foo
.
    my $f = B::svref_2object(*FOO{FORMAT});
    isa_ok $f, 'B::FM';
    can_ok $f, 'LINES';
}

my $sub1 = sub {die};
{ no warnings 'once'; no strict; *Peel:: = *{"Pe\0e\x{142}::"} }
my $sub2 = eval 'package Peel; sub {die}';
my $cop = B::svref_2object($sub1)->ROOT->first->first;
my $bobby = B::svref_2object($sub2)->ROOT->first->first;
is $cop->stash->object_2svref, \%main::, 'COP->stash';
is $cop->stashpv, 'main', 'COP->stashpv';

SKIP: {
    skip "no nulls in packages before 5.17", 1 if $] < 5.017;
    is $bobby->stashpv, "Pe\0e\x{142}", 'COP->stashpv with utf8 and nulls';
}

SKIP: {
    skip "no stashoff", 2 if $] < 5.017 || !$Config::Config{useithreads};
    like $cop->stashoff, qr/^[1-9]\d*\z/a, 'COP->stashoff';
    isnt $cop->stashoff, $bobby->stashoff,
	'different COP->stashoff for different stashes';
}


# Test $B::overlay
{
    my $methods = {
	BINOP =>  [ qw(last) ],
	COP   =>  [ qw(arybase cop_seq file filegv hints hints_hash io
		       label line stash stashpv
		       stashoff warnings) ],
	LISTOP => [ qw(children) ],
	LOGOP =>  [ qw(other) ],
	LOOP  =>  [ qw(lastop nextop redoop) ],
	OP    =>  [ qw(desc flags name next opt ppaddr private sibling
		       size spare targ type) ],
	PADOP =>  [ qw(gv padix sv) ],
	PMOP  =>  [ qw(code_list pmflags pmoffset pmreplroot pmreplstart pmstash pmstashpv precomp reflags) ],
	PVOP  =>  [ qw(pv) ],
	SVOP  =>  [ qw(gv sv) ],
	UNOP  =>  [ qw(first) ],
    };

    my $overlay = {};
    my $op = B::svref_2object(sub { my $x = 1 })->ROOT;

    for my $class (sort keys %$methods) {
	for my $meth (@{$methods->{$class}}) {
	    my $full = "B::${class}::$meth";
	    die "Duplicate method '$full'\n"
		if grep $_ eq $full, @{$overlay->{$meth}};
	    push @{$overlay->{$meth}}, "B::${class}::$meth";
	}
    }

    {
	local $B::overlay; # suppress 'used once' warning
	local $B::overlay = { $$op => $overlay };

	for my $class (sort keys %$methods) {
	    bless $op, "B::$class"; # naughty
	    for my $meth (@{$methods->{$class}}) {
		if ($op->can($meth)) {
		    my $list = $op->$meth;
		    ok(defined $list
			    && ref($list) eq "ARRAY"
			    && grep($_ eq "B::${class}::$meth", @$list),
			"overlay: B::$class $meth");
		}
		else {
		    pass("overlay: B::$class $meth (skipped; no method)");
		}
	    }
	}
    }
    # B::overlay should be disabled again here
    is($op->name, "leavesub", "overlay: orig name");
}

{ # [perl #118525]
    {
        sub foo {}
	my $cv = B::svref_2object(\&foo);
	ok($cv, "make a B::CV from a non-anon sub reference");
	isa_ok($cv, "B::CV");
	my $gv = $cv->GV;
	ok($gv, "we get a GV from a GV on a normal sub");
	isa_ok($gv, "B::GV");
	is($gv->NAME, "foo", "check the GV name");
      SKIP:
	{ # do we need these version checks?
	    skip "no HEK before 5.18", 1 if $] < 5.018;
	    is($cv->NAME_HEK, undef, "no hek for a global sub");
	}
    }

SKIP:
    {
        skip "no HEK before 5.18", 4 if $] < 5.018;
        eval <<'EOS'
    {
        use feature 'lexical_subs';
        no warnings 'experimental::lexical_subs';
        my sub bar {};
        my $cv = B::svref_2object(\&bar);
        ok($cv, "make a B::CV from a lexical sub reference");
        isa_ok($cv, "B::CV");
        my $gv = $cv->GV;
        isa_ok($gv, "B::SPECIAL", "GV on a lexical sub");
        my $hek = $cv->NAME_HEK;
        is($hek, "bar", "check the NAME_HEK");
    }
    1;
EOS
	  or die "lexical_subs test failed to compile: $@";
    }
}

{ # [perl #120535]
    my %h = ( "\x{100}" => 1 );
    my $b = B::svref_2object(\%h);
    my ($k, $v) = $b->ARRAY;
    is($k, "\x{100}", "check utf8 preserved by B::HV::ARRAY");
}

done_testing();
