#!perl

BEGIN {
    if ($ENV{PERL_CORE}) {
	chdir('t') if -d 't';
	@INC = ('.', '../lib', '../ext/B/t');
    } else {
	unshift @INC, 't';
	push @INC, "../../t";
    }
    require Config;
    if (($Config::Config{'extensions'} !~ /\bB\b/) ){
        print "1..0 # Skip -- Perl configured without B module\n";
        exit 0;
    }
    # require 'test.pl'; # now done by OptreeCheck
}

use OptreeCheck;	# ALSO DOES @ARGV HANDLING !!!!!!
use Config;

my $tests = 30;
plan tests => $tests;
SKIP: {
skip "no perlio in this build", $tests unless $Config::Config{useperlio};

#################################

use constant {		# see also t/op/gv.t line 282
    myaref	=> [ 1,2,3 ],
    myfl	=> 1.414213,
    myglob	=> \*STDIN,
    myhref	=> { a	=> 1 },
    myint	=> 42,
    myrex	=> qr/foo/,
    mystr	=> 'hithere',
    mysub	=> \&ok,
    myundef	=> undef,
    myunsub	=> \&nosuch,
};

sub myyes() { 1==1 }
sub myno () { return 1!=1 }
sub pi () { 3.14159 };

my $RV_class = $] >= 5.011 ? 'IV' : 'RV';

my $want = {	# expected types, how value renders in-line, todos (maybe)
    mystr	=> [ 'PV', '"'.mystr.'"' ],
    myhref	=> [ $RV_class, '\\\\HASH'],
    pi		=> [ 'NV', pi ],
    myglob	=> [ $RV_class, '\\\\' ],
    mysub	=> [ $RV_class, '\\\\' ],
    myunsub	=> [ $RV_class, '\\\\' ],
    # these are not inlined, at least not per BC::Concise
    #myyes	=> [ $RV_class, ],
    #myno	=> [ $RV_class, ],
    $] > 5.009 ? (
    myaref	=> [ $RV_class, '\\\\' ],
    myfl	=> [ 'NV', myfl ],
    myint	=> [ 'IV', myint ],
    $] >= 5.011 ? (
    myrex	=> [ $RV_class, '\\\\"\\(?-xism:Foo\\)"' ],
    ) : (
    myrex	=> [ $RV_class, '\\\\' ],
    ),
    myundef	=> [ 'NULL', ],
    ) : (
    myaref	=> [ 'PVIV', '' ],
    myfl	=> [ 'PVNV', myfl ],
    myint	=> [ 'PVIV', myint ],
    myrex	=> [ 'PVNV', '' ],
    myundef	=> [ 'PVIV', ],
    )
};

use constant WEEKDAYS
    => qw ( Sunday Monday Tuesday Wednesday Thursday Friday Saturday );


$::{napier} = \2.71828;	# counter-example (doesn't get optimized).
eval "sub napier ();";


# should be able to undefine constant::import here ???
INIT { 
    # eval 'sub constant::import () {}';
    # undef *constant::import::{CODE};
};

#################################
pass("RENDER CONSTANT SUBS RETURNING SCALARS");

for $func (sort keys %$want) {
    # no strict 'refs';	# why not needed ?
    checkOptree ( name      => "$func() as a coderef",
		  code      => \&{$func},
		  noanchors => 1,
		  expect    => <<EOT_EOT, expect_nt => <<EONT_EONT);
 is a constant sub, optimized to a $want->{$func}[0]
EOT_EOT
 is a constant sub, optimized to a $want->{$func}[0]
EONT_EONT

}

pass("RENDER CALLS TO THOSE CONSTANT SUBS");

for $func (sort keys %$want) {
    # print "# doing $func\n";
    checkOptree ( name    => "call $func",
		  code    => "$func",
		  ($want->{$func}[2]) ? ( todo => $want->{$func}[2]) : (),
		  bc_opts => '-nobanner',
		  expect  => <<EOT_EOT, expect_nt => <<EONT_EONT);
3  <1> leavesub[2 refs] K/REFC,1 ->(end)
-     <\@> lineseq KP ->3
1        <;> dbstate(main 833 (eval 44):1) v ->2
2        <\$> const[$want->{$func}[0] $want->{$func}[1]] s ->3
EOT_EOT
3  <1> leavesub[2 refs] K/REFC,1 ->(end)
-     <\@> lineseq KP ->3
1        <;> dbstate(main 833 (eval 44):1) v ->2
2        <\$> const($want->{$func}[0] $want->{$func}[1]) s ->3
EONT_EONT

}

##############
pass("MORE TESTS");

checkOptree ( name	=> 'myyes() as coderef',
	      code	=> sub () { 1==1 },
	      noanchors => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
 is a constant sub, optimized to a SPECIAL
EOT_EOT
 is a constant sub, optimized to a SPECIAL
EONT_EONT


checkOptree ( name	=> 'myyes() as coderef',
	      prog	=> 'sub a() { 1==1 }; print a',
	      noanchors => 1,
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 6  <@> leave[1 ref] vKP/REFC ->(end)
# 1     <0> enter ->2
# 2     <;> nextstate(main 2 -e:1) v:>,<,%,{ ->3
# 5     <@> print vK ->6
# 3        <0> pushmark s ->4
# 4        <$> const[SPECIAL sv_yes] s ->5
EOT_EOT
# 6  <@> leave[1 ref] vKP/REFC ->(end)
# 1     <0> enter ->2
# 2     <;> nextstate(main 2 -e:1) v:>,<,%,{ ->3
# 5     <@> print vK ->6
# 3        <0> pushmark s ->4
# 4        <$> const(SPECIAL sv_yes) s ->5
EONT_EONT


# Need to do this as a prog, not code, as only the first constant to use
# PL_sv_no actually gets to use the real thing - every one following is
# copied.
checkOptree ( name	=> 'myno() as coderef',
	      prog	=> 'sub a() { 1!=1 }; print a',
	      noanchors => 1,
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 6  <@> leave[1 ref] vKP/REFC ->(end)
# 1     <0> enter ->2
# 2     <;> nextstate(main 2 -e:1) v:>,<,%,{ ->3
# 5     <@> print vK ->6
# 3        <0> pushmark s ->4
# 4        <$> const[SPECIAL sv_no] s ->5
EOT_EOT
# 6  <@> leave[1 ref] vKP/REFC ->(end)
# 1     <0> enter ->2
# 2     <;> nextstate(main 2 -e:1) v:>,<,%,{ ->3
# 5     <@> print vK ->6
# 3        <0> pushmark s ->4
# 4        <$> const(SPECIAL sv_no) s ->5
EONT_EONT


my ($expect, $expect_nt) = (<<'EOT_EOT', <<'EONT_EONT');
# 3  <1> leavesub[2 refs] K/REFC,1 ->(end)
# -     <@> lineseq K ->3
# 1        <;> nextstate(constant 61 constant.pm:118) v:*,& ->2
# 2        <0> padav[@list:FAKE:m:96] ->3
EOT_EOT
# 3  <1> leavesub[2 refs] K/REFC,1 ->(end)
# -     <@> lineseq K ->3
# 1        <;> nextstate(constant 61 constant.pm:118) v:*,& ->2
# 2        <0> padav[@list:FAKE:m:71] ->3
EONT_EONT

if($] < 5.009) {
    # 5.8.x doesn't add the m flag to padav
    s/FAKE:m:\d+/FAKE/ foreach ($expect, $expect_nt);
}

checkOptree ( name	=> 'constant sub returning list',
	      code	=> \&WEEKDAYS,
	      noanchors => 1,
	      expect => $expect, expect_nt => $expect_nt);


sub printem {
    printf "myint %d mystr %s myfl %f pi %f\n"
	, myint, mystr, myfl, pi;
}

my ($expect, $expect_nt) = (<<'EOT_EOT', <<'EONT_EONT');
# 9  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->9
# 1        <;> nextstate(main 635 optree_constants.t:163) v:>,<,% ->2
# 8        <@> prtf sK ->9
# 2           <0> pushmark s ->3
# 3           <$> const[PV "myint %d mystr %s myfl %f pi %f\n"] s ->4
# 4           <$> const[IV 42] s ->5
# 5           <$> const[PV "hithere"] s ->6
# 6           <$> const[NV 1.414213] s ->7
# 7           <$> const[NV 3.14159] s ->8
EOT_EOT
# 9  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->9
# 1        <;> nextstate(main 635 optree_constants.t:163) v:>,<,% ->2
# 8        <@> prtf sK ->9
# 2           <0> pushmark s ->3
# 3           <$> const(PV "myint %d mystr %s myfl %f pi %f\n") s ->4
# 4           <$> const(IV 42) s ->5
# 5           <$> const(PV "hithere") s ->6
# 6           <$> const(NV 1.414213) s ->7
# 7           <$> const(NV 3.14159) s ->8
EONT_EONT

if($] < 5.009) {
    # 5.8.x's use constant has larger types
    foreach ($expect, $expect_nt) {
	s/IV 42/PV$&/;
	s/NV 1.41/PV$&/;
    }
}

checkOptree ( name	=> 'call many in a print statement',
	      code	=> \&printem,
	      strip_open_hints => 1,
	      expect => $expect, expect_nt => $expect_nt);

} #skip

__END__

=head NB

Optimized constant subs are stored as bare scalars in the stash
(package hash), which formerly held only GVs (typeglobs).

But you cant create them manually - you cant assign a scalar to a
stash element, and expect it to work like a constant-sub, even if you
provide a prototype.

This is a feature; alternative is too much action-at-a-distance.  The
following test demonstrates - napier is not seen as a function at all,
much less an optimized one.

=cut

checkOptree ( name	=> 'not evertnapier',
	      code	=> \&napier,
	      noanchors => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
 has no START
EOT_EOT
 has no START
EONT_EONT


