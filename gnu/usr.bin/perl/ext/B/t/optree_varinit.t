#!perl

BEGIN {
    chdir 't';
    @INC = ('../lib', '../ext/B/t');
    require Config;
    if (($Config::Config{'extensions'} !~ /\bB\b/) ){
        print "1..0 # Skip -- Perl configured without B module\n";
        exit 0;
    }
    require './test.pl';
}
use OptreeCheck;
use Config;
plan tests	=> 22;
SKIP: {
skip "no perlio in this build", 22 unless $Config::Config{useperlio};

pass("OPTIMIZER TESTS - VAR INITIALIZATION");

checkOptree ( name	=> 'sub {my $a}',
	      bcopts	=> '-exec',
	      code	=> sub {my $a},
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 45 optree.t:23) v
# 2  <0> padsv[$a:45,46] M/LVINTRO
# 3  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 45 optree.t:23) v
# 2  <0> padsv[$a:45,46] M/LVINTRO
# 3  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

checkOptree ( name	=> '-exec sub {my $a}',
	      bcopts	=> '-exec',
	      code	=> sub {my $a},
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 49 optree.t:52) v
# 2  <0> padsv[$a:49,50] M/LVINTRO
# 3  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 49 optree.t:45) v
# 2  <0> padsv[$a:49,50] M/LVINTRO
# 3  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

checkOptree ( name	=> 'sub {our $a}',
	      bcopts	=> '-exec',
	      code	=> sub {our $a},
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
1  <;> nextstate(main 21 optree.t:47) v
2  <#> gvsv[*a] s/OURINTR
3  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 51 optree.t:56) v
# 2  <$> gvsv(*a) s/OURINTR
# 3  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

checkOptree ( name	=> 'sub {local $a}',
	      bcopts	=> '-exec',
	      code	=> sub {local $a},
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
1  <;> nextstate(main 23 optree.t:57) v
2  <#> gvsv[*a] s/LVINTRO
3  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 53 optree.t:67) v
# 2  <$> gvsv(*a) s/LVINTRO
# 3  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

checkOptree ( name	=> 'my $a',
	      prog	=> 'my $a',
	      bcopts	=> '-basic',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 4  <@> leave[1 ref] vKP/REFC ->(end)
# 1     <0> enter ->2
# 2     <;> nextstate(main 1 -e:1) v ->3
# 3     <0> padsv[$a:1,2] vM/LVINTRO ->4
EOT_EOT
# 4  <@> leave[1 ref] vKP/REFC ->(end)
# 1     <0> enter ->2
# 2     <;> nextstate(main 1 -e:1) v ->3
# 3     <0> padsv[$a:1,2] vM/LVINTRO ->4
EONT_EONT

checkOptree ( name	=> 'our $a',
	      prog	=> 'our $a',
	      bcopts	=> '-basic',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
4  <@> leave[1 ref] vKP/REFC ->(end)
1     <0> enter ->2
2     <;> nextstate(main 1 -e:1) v ->3
-     <1> ex-rv2sv vK/17 ->4
3        <#> gvsv[*a] s/OURINTR ->4
EOT_EOT
# 4  <@> leave[1 ref] vKP/REFC ->(end)
# 1     <0> enter ->2
# 2     <;> nextstate(main 1 -e:1) v ->3
# -     <1> ex-rv2sv vK/17 ->4
# 3        <$> gvsv(*a) s/OURINTR ->4
EONT_EONT

checkOptree ( name	=> 'local $a',
	      prog	=> 'local $a',
	      bcopts	=> '-basic',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
4  <@> leave[1 ref] vKP/REFC ->(end)
1     <0> enter ->2
2     <;> nextstate(main 1 -e:1) v ->3
-     <1> ex-rv2sv vKM/129 ->4
3        <#> gvsv[*a] s/LVINTRO ->4
EOT_EOT
# 4  <@> leave[1 ref] vKP/REFC ->(end)
# 1     <0> enter ->2
# 2     <;> nextstate(main 1 -e:1) v ->3
# -     <1> ex-rv2sv vKM/129 ->4
# 3        <$> gvsv(*a) s/LVINTRO ->4
EONT_EONT

pass("MY, OUR, LOCAL, BOTH SUB AND MAIN, = undef");

checkOptree ( name	=> 'sub {my $a=undef}',
	      code	=> sub {my $a=undef},
	      bcopts	=> '-basic',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
3  <1> leavesub[1 ref] K/REFC,1 ->(end)
-     <@> lineseq KP ->3
1        <;> nextstate(main 24 optree.t:99) v ->2
2        <0> padsv[$a:24,25] sRM*/LVINTRO ->3
EOT_EOT
# 3  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->3
# 1        <;> nextstate(main 54 optree.t:149) v ->2
# 2        <0> padsv[$a:54,55] sRM*/LVINTRO ->3
EONT_EONT

checkOptree ( name	=> 'sub {our $a=undef}',
	      code	=> sub {our $a=undef},
	      note	=> 'the global must be reset',
	      bcopts	=> '-basic',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
5  <1> leavesub[1 ref] K/REFC,1 ->(end)
-     <@> lineseq KP ->5
1        <;> nextstate(main 26 optree.t:109) v ->2
4        <2> sassign sKS/2 ->5
2           <0> undef s ->3
-           <1> ex-rv2sv sKRM*/17 ->4
3              <#> gvsv[*a] s/OURINTR ->4
EOT_EOT
# 5  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->5
# 1        <;> nextstate(main 446 optree_varinit.t:137) v ->2
# 4        <2> sassign sKS/2 ->5
# 2           <0> undef s ->3
# -           <1> ex-rv2sv sKRM*/17 ->4
# 3              <$> gvsv(*a) s/OURINTR ->4
EONT_EONT

checkOptree ( name	=> 'sub {local $a=undef}',
	      code	=> sub {local $a=undef},
	      note	=> 'local not used enough to bother',
	      bcopts	=> '-basic',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
5  <1> leavesub[1 ref] K/REFC,1 ->(end)
-     <@> lineseq KP ->5
1        <;> nextstate(main 28 optree.t:122) v ->2
4        <2> sassign sKS/2 ->5
2           <0> undef s ->3
-           <1> ex-rv2sv sKRM*/129 ->4
3              <#> gvsv[*a] s/LVINTRO ->4
EOT_EOT
# 5  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->5
# 1        <;> nextstate(main 58 optree.t:141) v ->2
# 4        <2> sassign sKS/2 ->5
# 2           <0> undef s ->3
# -           <1> ex-rv2sv sKRM*/129 ->4
# 3              <$> gvsv(*a) s/LVINTRO ->4
EONT_EONT

checkOptree ( name	=> 'my $a=undef',
	      prog	=> 'my $a=undef',
	      bcopts	=> '-basic',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
4  <@> leave[1 ref] vKP/REFC ->(end)
1     <0> enter ->2
2     <;> nextstate(main 1 -e:1) v ->3
3     <0> padsv[$a:1,2] vRM*/LVINTRO ->4
EOT_EOT
# 4  <@> leave[1 ref] vKP/REFC ->(end)
# 1     <0> enter ->2
# 2     <;> nextstate(main 1 -e:1) v ->3
# 3     <0> padsv[$a:1,2] vRM*/LVINTRO ->4
EONT_EONT

checkOptree ( name	=> 'our $a=undef',
	      prog	=> 'our $a=undef',
	      note	=> 'global must be reassigned',
	      bcopts	=> '-basic',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
6  <@> leave[1 ref] vKP/REFC ->(end)
1     <0> enter ->2
2     <;> nextstate(main 1 -e:1) v ->3
5     <2> sassign vKS/2 ->6
3        <0> undef s ->4
-        <1> ex-rv2sv sKRM*/17 ->5
4           <#> gvsv[*a] s/OURINTR ->5
EOT_EOT
# 6  <@> leave[1 ref] vKP/REFC ->(end)
# 1     <0> enter ->2
# 2     <;> nextstate(main 1 -e:1) v ->3
# 5     <2> sassign vKS/2 ->6
# 3        <0> undef s ->4
# -        <1> ex-rv2sv sKRM*/17 ->5
# 4           <$> gvsv(*a) s/OURINTR ->5
EONT_EONT

checkOptree ( name	=> 'local $a=undef',
	      prog	=> 'local $a=undef',
	      note	=> 'locals are rare, probly not worth doing',
	      bcopts	=> '-basic',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
6  <@> leave[1 ref] vKP/REFC ->(end)
1     <0> enter ->2
2     <;> nextstate(main 1 -e:1) v ->3
5     <2> sassign vKS/2 ->6
3        <0> undef s ->4
-        <1> ex-rv2sv sKRM*/129 ->5
4           <#> gvsv[*a] s/LVINTRO ->5
EOT_EOT
# 6  <@> leave[1 ref] vKP/REFC ->(end)
# 1     <0> enter ->2
# 2     <;> nextstate(main 1 -e:1) v ->3
# 5     <2> sassign vKS/2 ->6
# 3        <0> undef s ->4
# -        <1> ex-rv2sv sKRM*/129 ->5
# 4           <$> gvsv(*a) s/LVINTRO ->5
EONT_EONT

checkOptree ( name	=> 'sub {my $a=()}',
	      code	=> sub {my $a=()},
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
1  <;> nextstate(main -439 optree.t:105) v
2  <0> stub sP
3  <0> padsv[$a:-439,-438] sRM*/LVINTRO
4  <2> sassign sKS/2
5  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 438 optree_varinit.t:247) v
# 2  <0> stub sP
# 3  <0> padsv[$a:438,439] sRM*/LVINTRO
# 4  <2> sassign sKS/2
# 5  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

checkOptree ( name	=> 'sub {our $a=()}',
	      code	=> sub {our $a=()},
              #todo	=> 'probly not worth doing',
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
1  <;> nextstate(main 31 optree.t:177) v
2  <0> stub sP
3  <#> gvsv[*a] s/OURINTR
4  <2> sassign sKS/2
5  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 440 optree_varinit.t:262) v
# 2  <0> stub sP
# 3  <$> gvsv(*a) s/OURINTR
# 4  <2> sassign sKS/2
# 5  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

checkOptree ( name	=> 'sub {local $a=()}',
	      code	=> sub {local $a=()},
              #todo	=> 'probly not worth doing',
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
1  <;> nextstate(main 33 optree.t:190) v
2  <0> stub sP
3  <#> gvsv[*a] s/LVINTRO
4  <2> sassign sKS/2
5  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 63 optree.t:225) v
# 2  <0> stub sP
# 3  <$> gvsv(*a) s/LVINTRO
# 4  <2> sassign sKS/2
# 5  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

checkOptree ( name	=> 'my $a=()',
	      prog	=> 'my $a=()',
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
1  <0> enter 
2  <;> nextstate(main 1 -e:1) v
3  <0> stub sP
4  <0> padsv[$a:1,2] sRM*/LVINTRO
5  <2> sassign vKS/2
6  <@> leave[1 ref] vKP/REFC
EOT_EOT
# 1  <0> enter 
# 2  <;> nextstate(main 1 -e:1) v
# 3  <0> stub sP
# 4  <0> padsv[$a:1,2] sRM*/LVINTRO
# 5  <2> sassign vKS/2
# 6  <@> leave[1 ref] vKP/REFC
EONT_EONT

checkOptree ( name	=> 'our $a=()',
	      prog	=> 'our $a=()',
              #todo	=> 'probly not worth doing',
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
1  <0> enter 
2  <;> nextstate(main 1 -e:1) v
3  <0> stub sP
4  <#> gvsv[*a] s/OURINTR
5  <2> sassign vKS/2
6  <@> leave[1 ref] vKP/REFC
EOT_EOT
# 1  <0> enter 
# 2  <;> nextstate(main 1 -e:1) v
# 3  <0> stub sP
# 4  <$> gvsv(*a) s/OURINTR
# 5  <2> sassign vKS/2
# 6  <@> leave[1 ref] vKP/REFC
EONT_EONT

checkOptree ( name	=> 'local $a=()',
	      prog	=> 'local $a=()',
              #todo	=> 'probly not worth doing',
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
1  <0> enter 
2  <;> nextstate(main 1 -e:1) v
3  <0> stub sP
4  <#> gvsv[*a] s/LVINTRO
5  <2> sassign vKS/2
6  <@> leave[1 ref] vKP/REFC
EOT_EOT
# 1  <0> enter 
# 2  <;> nextstate(main 1 -e:1) v
# 3  <0> stub sP
# 4  <$> gvsv(*a) s/LVINTRO
# 5  <2> sassign vKS/2
# 6  <@> leave[1 ref] vKP/REFC
EONT_EONT

checkOptree ( name	=> 'my ($a,$b)=()',
	      prog	=> 'my ($a,$b)=()',
              #todo	=> 'probly not worth doing',
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <0> enter 
# 2  <;> nextstate(main 1 -e:1) v
# 3  <0> pushmark s
# 4  <0> pushmark sRM*/128
# 5  <0> padsv[$a:1,2] lRM*/LVINTRO
# 6  <0> padsv[$b:1,2] lRM*/LVINTRO
# 7  <2> aassign[t3] vKS
# 8  <@> leave[1 ref] vKP/REFC
EOT_EOT
# 1  <0> enter 
# 2  <;> nextstate(main 1 -e:1) v
# 3  <0> pushmark s
# 4  <0> pushmark sRM*/128
# 5  <0> padsv[$a:1,2] lRM*/LVINTRO
# 6  <0> padsv[$b:1,2] lRM*/LVINTRO
# 7  <2> aassign[t3] vKS
# 8  <@> leave[1 ref] vKP/REFC
EONT_EONT

} #skip

__END__

