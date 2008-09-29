#!perl

BEGIN {
    if ($ENV{PERL_CORE}){
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
}
use OptreeCheck;
use Config;
plan tests => 2;

SKIP: {
skip "no perlio in this build", 1 unless $Config::Config{useperlio};

# The regression this is testing is that the first aelemfast, derived
# from a lexical array, is supposed to be a BASEOP "<0>", while the
# second, from a global, is an SVOP "<$>" or a PADOP "<#>" depending
# on threading. In buggy versions, both showed up as SVOPs/PADOPs. See
# B.xs:cc_opclass() for the relevant code.

checkOptree ( name	=> 'OP_AELEMFAST opclass',
	      code	=> sub { my @x; our @y; $x[0] + $y[0]},
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# a  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->a
# 1        <;> nextstate(main 634 optree_misc.t:25) v:>,<,% ->2
# 2        <0> padav[@x:634,636] vM/LVINTRO ->3
# 3        <;> nextstate(main 635 optree_misc.t:25) v:>,<,% ->4
# 5        <1> rv2av[t4] vK/OURINTR,1 ->6
# 4           <#> gv[*y] s ->5
# 6        <;> nextstate(main 636 optree_misc.t:25) v:>,<,%,{ ->7
# 9        <2> add[t6] sK/2 ->a
# -           <1> ex-aelem sK/2 ->8
# 7              <0> aelemfast[@x:634,636] sR* ->8
# -              <0> ex-const s ->-
# -           <1> ex-aelem sK/2 ->9
# -              <1> ex-rv2av sKR/1 ->-
# 8                 <#> aelemfast[*y] s ->9
# -              <0> ex-const s ->-
EOT_EOT
# a  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->a
# 1        <;> nextstate(main 634 optree_misc.t:27) v:>,<,% ->2
# 2        <0> padav[@x:634,636] vM/LVINTRO ->3
# 3        <;> nextstate(main 635 optree_misc.t:27) v:>,<,% ->4
# 5        <1> rv2av[t3] vK/OURINTR,1 ->6
# 4           <$> gv(*y) s ->5
# 6        <;> nextstate(main 636 optree_misc.t:27) v:>,<,%,{ ->7
# 9        <2> add[t4] sK/2 ->a
# -           <1> ex-aelem sK/2 ->8
# 7              <0> aelemfast[@x:634,636] sR* ->8
# -              <0> ex-const s ->-
# -           <1> ex-aelem sK/2 ->9
# -              <1> ex-rv2av sKR/1 ->-
# 8                 <$> aelemfast(*y) s ->9
# -              <0> ex-const s ->-
EONT_EONT


} #skip

my $t = <<'EOT_EOT';
# 8  <@> leave[1 ref] vKP/REFC ->(end)
# 1     <0> enter ->2
# 2     <;> nextstate(main 1 -e:1) v:>,<,%,{ ->3
# 7     <2> sassign vKS/2 ->8
# 5        <@> index[t2] sK/2 ->6
# -           <0> ex-pushmark s ->3
# 3           <$> const[PV "foo"] s ->4
# 4           <$> const[GV "foo"] s ->5
# -        <1> ex-rv2sv sKRM*/1 ->7
# 6           <#> gvsv[*_] s ->7
EOT_EOT
my $nt = <<'EONT_EONT';
# 8  <@> leave[1 ref] vKP/REFC ->(end)
# 1     <0> enter ->2
# 2     <;> nextstate(main 1 -e:1) v:>,<,%,{ ->3
# 7     <2> sassign vKS/2 ->8
# 5        <@> index[t1] sK/2 ->6
# -           <0> ex-pushmark s ->3
# 3           <$> const(PV "foo") s ->4
# 4           <$> const(GV "foo") s ->5
# -        <1> ex-rv2sv sKRM*/1 ->7
# 6           <$> gvsv(*_) s ->7
EONT_EONT

if ($] < 5.009) {
    $t =~ s/GV /BM /;
    $nt =~ s/GV /BM /;
} 

checkOptree ( name      => 'index and PVBM',
	      prog	=> '$_ = index q(foo), q(foo)',
	      strip_open_hints => 1,
	      expect	=> $t,  expect_nt => $nt);

__END__

