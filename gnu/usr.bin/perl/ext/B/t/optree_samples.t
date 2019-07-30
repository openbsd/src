#!perl

BEGIN {
    unshift @INC, 't';
    require Config;
    if (($Config::Config{'extensions'} !~ /\bB\b/) ){
        print "1..0 # Skip -- Perl configured without B module\n";
        exit 0;
    }
    if (!$Config::Config{useperlio}) {
        print "1..0 # Skip -- need perlio to walk the optree\n";
        exit 0;
    }
}
use OptreeCheck;
use Config;
plan tests	=> 46;

pass("GENERAL OPTREE EXAMPLES");

pass("IF,THEN,ELSE, ?:");

checkOptree ( name	=> '-basic sub {if shift print then,else}',
	      bcopts	=> '-basic',
	      code	=> sub { if (shift) { print "then" }
				 else       { print "else" }
			     },
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 7  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->7
# 1        <;> nextstate(main 665 optree_samples.t:24) v:>,<,% ->2
# -        <1> null K/1 ->-
# 3           <|> cond_expr(other->4) K/1 ->8
# 2              <0> shift s* ->3
# -              <@> scope K ->-
# -                 <;> ex-nextstate(main 1594 optree_samples.t:25) v:>,<,% ->4
# 6                 <@> print sK ->7
# 4                    <0> pushmark s ->5
# 5                    <$> const[PV "then"] s ->6
# d              <@> leave KP ->7
# 8                 <0> enter ->9
# 9                 <;> nextstate(main 663 optree_samples.t:25) v:>,<,% ->a
# c                 <@> print sK ->d
# a                    <0> pushmark s ->b
# b                    <$> const[PV "else"] s ->c
EOT_EOT
# 7  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->7
# 1        <;> nextstate(main 665 optree_samples.t:24) v:>,<,% ->2
# -        <1> null K/1 ->-
# 3           <|> cond_expr(other->4) K/1 ->8
# 2              <0> shift s* ->3
# -              <@> scope K ->-
# -                 <;> ex-nextstate(main 1594 optree_samples.t:25) v:>,<,% ->4
# 6                 <@> print sK ->7
# 4                    <0> pushmark s ->5
# 5                    <$> const(PV "then") s ->6
# d              <@> leave KP ->7
# 8                 <0> enter ->9
# 9                 <;> nextstate(main 663 optree_samples.t:25) v:>,<,% ->a
# c                 <@> print sK ->d
# a                    <0> pushmark s ->b
# b                    <$> const(PV "else") s ->c
EONT_EONT

checkOptree ( name	=> '-basic (see above, with my $a = shift)',
	      bcopts	=> '-basic',
	      code	=> sub { my $a = shift;
				 if ($a) { print "foo" }
				 else    { print "bar" }
			     },
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# b  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->b
# 1        <;> nextstate(main 666 optree_samples.t:70) v:>,<,% ->2
# 4        <2> sassign vKS/2 ->5
# 2           <0> shift s* ->3
# 3           <0> padsv[$a:666,670] sRM*/LVINTRO ->4
# 5        <;> nextstate(main 670 optree_samples.t:71) v:>,<,% ->6
# -        <1> null K/1 ->-
# 7           <|> cond_expr(other->8) K/1 ->c
# 6              <0> padsv[$a:666,670] s ->7
# -              <@> scope K ->-
# -                 <;> ex-nextstate(main 1603 optree_samples.t:70) v:>,<,% ->8
# a                 <@> print sK ->b
# 8                    <0> pushmark s ->9
# 9                    <$> const[PV "foo"] s ->a
# h              <@> leave KP ->b
# c                 <0> enter ->d
# d                 <;> nextstate(main 668 optree_samples.t:72) v:>,<,% ->e
# g                 <@> print sK ->h
# e                    <0> pushmark s ->f
# f                    <$> const[PV "bar"] s ->g
EOT_EOT
# b  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->b
# 1        <;> nextstate(main 666 optree_samples.t:72) v:>,<,% ->2
# 4        <2> sassign vKS/2 ->5
# 2           <0> shift s* ->3
# 3           <0> padsv[$a:666,670] sRM*/LVINTRO ->4
# 5        <;> nextstate(main 670 optree_samples.t:73) v:>,<,% ->6
# -        <1> null K/1 ->-
# 7           <|> cond_expr(other->8) K/1 ->c
# 6              <0> padsv[$a:666,670] s ->7
# -              <@> scope K ->-
# -                 <;> ex-nextstate(main 1603 optree_samples.t:70) v:>,<,% ->8
# a                 <@> print sK ->b
# 8                    <0> pushmark s ->9
# 9                    <$> const(PV "foo") s ->a
# h              <@> leave KP ->b
# c                 <0> enter ->d
# d                 <;> nextstate(main 668 optree_samples.t:74) v:>,<,% ->e
# g                 <@> print sK ->h
# e                    <0> pushmark s ->f
# f                    <$> const(PV "bar") s ->g
EONT_EONT

checkOptree ( name	=> '-exec sub {if shift print then,else}',
	      bcopts	=> '-exec',
	      code	=> sub { if (shift) { print "then" }
				 else       { print "else" }
			     },
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 674 optree_samples.t:125) v:>,<,%
# 2  <0> shift s*
# 3  <|> cond_expr(other->4) K/1
# 4      <0> pushmark s
# 5      <$> const[PV "then"] s
# 6      <@> print sK
#            goto 7
# 8  <0> enter 
# 9  <;> nextstate(main 672 optree_samples.t:126) v:>,<,%
# a  <0> pushmark s
# b  <$> const[PV "else"] s
# c  <@> print sK
# d  <@> leave KP
# 7  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 674 optree_samples.t:129) v:>,<,%
# 2  <0> shift s*
# 3  <|> cond_expr(other->4) K/1
# 4      <0> pushmark s
# 5      <$> const(PV "then") s
# 6      <@> print sK
#            goto 7
# 8  <0> enter 
# 9  <;> nextstate(main 672 optree_samples.t:130) v:>,<,%
# a  <0> pushmark s
# b  <$> const(PV "else") s
# c  <@> print sK
# d  <@> leave KP
# 7  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

checkOptree ( name	=> '-exec (see above, with my $a = shift)',
	      bcopts	=> '-exec',
	      code	=> sub { my $a = shift;
				 if ($a) { print "foo" }
				 else    { print "bar" }
			     },
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 675 optree_samples.t:165) v:>,<,%
# 2  <0> shift s*
# 3  <0> padsv[$a:675,679] sRM*/LVINTRO
# 4  <2> sassign vKS/2
# 5  <;> nextstate(main 679 optree_samples.t:166) v:>,<,%
# 6  <0> padsv[$a:675,679] s
# 7  <|> cond_expr(other->8) K/1
# 8      <0> pushmark s
# 9      <$> const[PV "foo"] s
# a      <@> print sK
#            goto b
# c  <0> enter 
# d  <;> nextstate(main 677 optree_samples.t:167) v:>,<,%
# e  <0> pushmark s
# f  <$> const[PV "bar"] s
# g  <@> print sK
# h  <@> leave KP
# b  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 675 optree_samples.t:171) v:>,<,%
# 2  <0> shift s*
# 3  <0> padsv[$a:675,679] sRM*/LVINTRO
# 4  <2> sassign vKS/2
# 5  <;> nextstate(main 679 optree_samples.t:172) v:>,<,%
# 6  <0> padsv[$a:675,679] s
# 7  <|> cond_expr(other->8) K/1
# 8      <0> pushmark s
# 9      <$> const(PV "foo") s
# a      <@> print sK
#            goto b
# c  <0> enter 
# d  <;> nextstate(main 677 optree_samples.t:173) v:>,<,%
# e  <0> pushmark s
# f  <$> const(PV "bar") s
# g  <@> print sK
# h  <@> leave KP
# b  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

checkOptree ( name	=> '-exec sub { print (shift) ? "foo" : "bar" }',
	      code	=> sub { print (shift) ? "foo" : "bar" },
	      bcopts	=> '-exec',
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 680 optree_samples.t:213) v:>,<,%
# 2  <0> pushmark s
# 3  <0> shift s*
# 4  <@> print sK
# 5  <|> cond_expr(other->6) K/1
# 6      <$> const[PV "foo"] s
#            goto 7
# 8  <$> const[PV "bar"] s
# 7  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 680 optree_samples.t:221) v:>,<,%
# 2  <0> pushmark s
# 3  <0> shift s*
# 4  <@> print sK
# 5  <|> cond_expr(other->6) K/1
# 6      <$> const(PV "foo") s
#            goto 7
# 8  <$> const(PV "bar") s
# 7  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

pass ("FOREACH");

checkOptree ( name	=> '-exec sub { foreach (1..10) {print "foo $_"} }',
	      code	=> sub { foreach (1..10) {print "foo $_"} },
	      bcopts	=> '-exec',
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 443 optree.t:158) v:>,<,%
# 2  <0> pushmark s
# 3  <$> const[IV 1] s
# 4  <$> const[IV 10] s
# 5  <#> gv[*_] s
# 6  <{> enteriter(next->d last->g redo->7) KS/DEF
# e  <0> iter s
# f  <|> and(other->7) K/1
# 7      <;> nextstate(main 442 optree.t:158) v:>,<,%
# 8      <0> pushmark s
# 9      <$> const[PV "foo "] s
# a      <#> gvsv[*_] s
# b      <2> concat[t4] sK/2
# c      <@> print vK
# d      <0> unstack s
#            goto e
# g  <2> leaveloop K/2
# h  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 444 optree_samples.t:182) v:>,<,%
# 2  <0> pushmark s
# 3  <$> const(IV 1) s
# 4  <$> const(IV 10) s
# 5  <$> gv(*_) s
# 6  <{> enteriter(next->d last->g redo->7) KS/DEF
# e  <0> iter s
# f  <|> and(other->7) K/1
# 7      <;> nextstate(main 443 optree_samples.t:182) v:>,<,%
# 8      <0> pushmark s
# 9      <$> const(PV "foo ") s
# a      <$> gvsv(*_) s
# b      <2> concat[t3] sK/2
# c      <@> print vK
# d      <0> unstack s
#            goto e
# g  <2> leaveloop K/2
# h  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

checkOptree ( name	=> '-basic sub { print "foo $_" foreach (1..10) }',
	      code	=> sub { print "foo $_" foreach (1..10) }, 
	      bcopts	=> '-basic',
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# g  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->g
# 1        <;> nextstate(main 445 optree.t:167) v:>,<,% ->2
# f        <2> leaveloop K/2 ->g
# 6           <{> enteriter(next->c last->f redo->7) KS/DEF ->d
# -              <0> ex-pushmark s ->2
# -              <1> ex-list lK ->5
# 2                 <0> pushmark s ->3
# 3                 <$> const[IV 1] s ->4
# 4                 <$> const[IV 10] s ->5
# 5              <#> gv[*_] s ->6
# -           <1> null K/1 ->f
# e              <|> and(other->7) K/1 ->f
# d                 <0> iter s ->e
# -                 <@> lineseq sK ->-
# b                    <@> print vK ->c
# 7                       <0> pushmark s ->8
# -                       <1> ex-stringify sK/1 ->b
# -                          <0> ex-pushmark s ->8
# a                          <2> concat[t2] sK/2 ->b
# 8                             <$> const[PV "foo "] s ->9
# -                             <1> ex-rv2sv sK/1 ->a
# 9                                <#> gvsv[*_] s ->a
# c                    <0> unstack s ->d
EOT_EOT
# g  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->g
# 1        <;> nextstate(main 446 optree_samples.t:192) v:>,<,% ->2
# f        <2> leaveloop K/2 ->g
# 6           <{> enteriter(next->c last->f redo->7) KS/DEF ->d
# -              <0> ex-pushmark s ->2
# -              <1> ex-list lK ->5
# 2                 <0> pushmark s ->3
# 3                 <$> const(IV 1) s ->4
# 4                 <$> const(IV 10) s ->5
# 5              <$> gv(*_) s ->6
# -           <1> null K/1 ->f
# e              <|> and(other->7) K/1 ->f
# d                 <0> iter s ->e
# -                 <@> lineseq sK ->-
# b                    <@> print vK ->c
# 7                       <0> pushmark s ->8
# -                       <1> ex-stringify sK/1 ->b
# -                          <0> ex-pushmark s ->8
# a                          <2> concat[t1] sK/2 ->b
# 8                             <$> const(PV "foo ") s ->9
# -                             <1> ex-rv2sv sK/1 ->a
# 9                                <$> gvsv(*_) s ->a
# c                    <0> unstack s ->d
EONT_EONT

checkOptree ( name	=> '-exec -e foreach (1..10) {print qq{foo $_}}',
	      prog	=> 'foreach (1..10) {print qq{foo $_}}',
	      bcopts	=> '-exec',
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <0> enter 
# 2  <;> nextstate(main 2 -e:1) v:>,<,%,{
# 3  <0> pushmark s
# 4  <$> const[IV 1] s
# 5  <$> const[IV 10] s
# 6  <#> gv[*_] s
# 7  <{> enteriter(next->e last->h redo->8) vKS/DEF
# f  <0> iter s
# g  <|> and(other->8) vK/1
# 8      <;> nextstate(main 1 -e:1) v:>,<,%
# 9      <0> pushmark s
# a      <$> const[PV "foo "] s
# b      <#> gvsv[*_] s
# c      <2> concat[t4] sK/2
# d      <@> print vK
# e      <0> unstack v
#            goto f
# h  <2> leaveloop vK/2
# i  <@> leave[1 ref] vKP/REFC
EOT_EOT
# 1  <0> enter 
# 2  <;> nextstate(main 2 -e:1) v:>,<,%,{
# 3  <0> pushmark s
# 4  <$> const(IV 1) s
# 5  <$> const(IV 10) s
# 6  <$> gv(*_) s
# 7  <{> enteriter(next->e last->h redo->8) vKS/DEF
# f  <0> iter s
# g  <|> and(other->8) vK/1
# 8      <;> nextstate(main 1 -e:1) v:>,<,%
# 9      <0> pushmark s
# a      <$> const(PV "foo ") s
# b      <$> gvsv(*_) s
# c      <2> concat[t3] sK/2
# d      <@> print vK
# e      <0> unstack v
#            goto f
# h  <2> leaveloop vK/2
# i  <@> leave[1 ref] vKP/REFC
EONT_EONT

checkOptree ( name	=> '-exec sub { print "foo $_" foreach (1..10) }',
	      code	=> sub { print "foo $_" foreach (1..10) }, 
	      bcopts	=> '-exec',
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 445 optree.t:167) v:>,<,%
# 2  <0> pushmark s
# 3  <$> const[IV 1] s
# 4  <$> const[IV 10] s
# 5  <#> gv[*_] s
# 6  <{> enteriter(next->c last->f redo->7) KS/DEF
# d  <0> iter s
# e  <|> and(other->7) K/1
# 7      <0> pushmark s
# 8      <$> const[PV "foo "] s
# 9      <#> gvsv[*_] s
# a      <2> concat[t2] sK/2
# b      <@> print vK
# c      <0> unstack s
#            goto d
# f  <2> leaveloop K/2
# g  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 447 optree_samples.t:252) v:>,<,%
# 2  <0> pushmark s
# 3  <$> const(IV 1) s
# 4  <$> const(IV 10) s
# 5  <$> gv(*_) s
# 6  <{> enteriter(next->c last->f redo->7) KS/DEF
# d  <0> iter s
# e  <|> and(other->7) K/1
# 7      <0> pushmark s
# 8      <$> const(PV "foo ") s
# 9      <$> gvsv(*_) s
# a      <2> concat[t1] sK/2
# b      <@> print vK
# c      <0> unstack s
#            goto d
# f  <2> leaveloop K/2
# g  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

pass("GREP: SAMPLES FROM PERLDOC -F GREP");

checkOptree ( name	=> '@foo = grep(!/^\#/, @bar)',
	      code	=> '@foo = grep(!/^\#/, @bar)',
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 496 (eval 20):1) v:{
# 2  <0> pushmark s
# 3  <0> pushmark s
# 4  <#> gv[*bar] s
# 5  <1> rv2av[t4] lKM/1
# 6  <@> grepstart lK
# 7  <|> grepwhile(other->8)[t5] lK
# 8      </> match(/"^#"/) s/RTIME
# 9      <1> not sK/1
#            goto 7
# a  <0> pushmark s
# b  <#> gv[*foo] s
# c  <1> rv2av[t2] lKRM*/1
# d  <2> aassign[t6] KS/COM_AGG
# e  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 496 (eval 20):1) v:{
# 2  <0> pushmark s
# 3  <0> pushmark s
# 4  <$> gv(*bar) s
# 5  <1> rv2av[t2] lKM/1
# 6  <@> grepstart lK
# 7  <|> grepwhile(other->8)[t3] lK
# 8      </> match(/"^\\#"/) s/RTIME
# 9      <1> not sK/1
#            goto 7
# a  <0> pushmark s
# b  <$> gv(*foo) s
# c  <1> rv2av[t1] lKRM*/1
# d  <2> aassign[t4] KS/COM_AGG
# e  <1> leavesub[1 ref] K/REFC,1
EONT_EONT


pass("MAP: SAMPLES FROM PERLDOC -F MAP");

checkOptree ( name	=> '%h = map { getkey($_) => $_ } @a',
	      code	=> '%h = map { getkey($_) => $_ } @a',
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 501 (eval 22):1) v:{
# 2  <0> pushmark s
# 3  <0> pushmark s
# 4  <#> gv[*a] s
# 5  <1> rv2av[t8] lKM/1
# 6  <@> mapstart lK*                 < 5.017002
# 6  <@> mapstart lK                  >=5.017002
# 7  <|> mapwhile(other->8)[t9] lK
# 8      <0> enter l
# 9      <;> nextstate(main 500 (eval 22):1) v:{
# a      <0> pushmark s
# b      <#> gvsv[*_] s
# c      <#> gv[*getkey] s/EARLYCV
# d      <1> entersub[t5] lKS/TARG
# e      <#> gvsv[*_] s
# f      <@> leave lKP
#            goto 7
# g  <0> pushmark s
# h  <#> gv[*h] s
# i  <1> rv2hv[t2] lKRM*/1         < 5.019006
# i  <1> rv2hv lKRM*/1             >=5.019006
# j  <2> aassign[t10] KS/COM_AGG
# k  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 501 (eval 22):1) v:{
# 2  <0> pushmark s
# 3  <0> pushmark s
# 4  <$> gv(*a) s
# 5  <1> rv2av[t3] lKM/1
# 6  <@> mapstart lK*                 < 5.017002
# 6  <@> mapstart lK                  >=5.017002
# 7  <|> mapwhile(other->8)[t4] lK
# 8      <0> enter l
# 9      <;> nextstate(main 500 (eval 22):1) v:{
# a      <0> pushmark s
# b      <$> gvsv(*_) s
# c      <$> gv(*getkey) s/EARLYCV
# d      <1> entersub[t2] lKS/TARG
# e      <$> gvsv(*_) s
# f      <@> leave lKP
#            goto 7
# g  <0> pushmark s
# h  <$> gv(*h) s
# i  <1> rv2hv[t1] lKRM*/1         < 5.019006
# i  <1> rv2hv lKRM*/1             >=5.019006
# j  <2> aassign[t5] KS/COM_AGG
# k  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

checkOptree ( name	=> '%h=(); for $_(@a){$h{getkey($_)} = $_}',
	      code	=> '%h=(); for $_(@a){$h{getkey($_)} = $_}',
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 505 (eval 24):1) v
# 2  <0> pushmark s
# 3  <0> pushmark s
# 4  <#> gv[*h] s
# 5  <1> rv2hv[t2] lKRM*/1         < 5.019006
# 5  <1> rv2hv lKRM*/1             >=5.019006
# 6  <2> aassign[t3] vKS
# 7  <;> nextstate(main 506 (eval 24):1) v:{
# 8  <0> pushmark sM
# 9  <#> gv[*a] s
# a  <1> rv2av[t6] sKRM/1
# b  <#> gv[*_] s
# c  <1> rv2gv sKRM/1
# d  <{> enteriter(next->o last->r redo->e) KS/DEF
# p  <0> iter s
# q  <|> and(other->e) K/1
# e      <;> nextstate(main 505 (eval 24):1) v:{
# f      <#> gvsv[*_] s
# g      <#> gv[*h] s
# h      <1> rv2hv sKR/1
# i      <0> pushmark s
# j      <#> gvsv[*_] s
# k      <#> gv[*getkey] s/EARLYCV
# l      <1> entersub[t10] sKS/TARG
# m      <2> helem sKRM*/2
# n      <2> sassign vKS/2
# o      <0> unstack s
#            goto p
# r  <2> leaveloop KP/2
# s  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 505 (eval 24):1) v
# 2  <0> pushmark s
# 3  <0> pushmark s
# 4  <$> gv(*h) s
# 5  <1> rv2hv[t1] lKRM*/1         < 5.019006
# 5  <1> rv2hv lKRM*/1             >=5.019006
# 6  <2> aassign[t2] vKS
# 7  <;> nextstate(main 506 (eval 24):1) v:{
# 8  <0> pushmark sM
# 9  <$> gv(*a) s
# a  <1> rv2av[t3] sKRM/1
# b  <$> gv(*_) s
# c  <1> rv2gv sKRM/1
# d  <{> enteriter(next->o last->r redo->e) KS/DEF
# p  <0> iter s
# q  <|> and(other->e) K/1
# e      <;> nextstate(main 505 (eval 24):1) v:{
# f      <$> gvsv(*_) s
# g      <$> gv(*h) s
# h      <1> rv2hv sKR/1
# i      <0> pushmark s
# j      <$> gvsv(*_) s
# k      <$> gv(*getkey) s/EARLYCV
# l      <1> entersub[t4] sKS/TARG
# m      <2> helem sKRM*/2
# n      <2> sassign vKS/2
# o      <0> unstack s
#            goto p
# r  <2> leaveloop KP/2
# s  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

checkOptree ( name	=> 'map $_+42, 10..20',
	      code	=> 'map $_+42, 10..20',
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 497 (eval 20):1) v
# 2  <0> pushmark s
# 3  <$> const[AV ] s
# 4  <1> rv2av lKPM/1
# 5  <@> mapstart K
# 6  <|> mapwhile(other->7)[t5] K
# 7      <#> gvsv[*_] s
# 8      <$> const[IV 42] s
# 9      <2> add[t2] sK/2
#            goto 6
# a  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 511 (eval 26):1) v
# 2  <0> pushmark s
# 3  <$> const(AV ) s
# 4  <1> rv2av lKPM/1
# 5  <@> mapstart K
# 6  <|> mapwhile(other->7)[t4] K
# 7      <$> gvsv(*_) s
# 8      <$> const(IV 42) s
# 9      <2> add[t1] sK/2
#            goto 6
# a  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

pass("CONSTANTS");

checkOptree ( name	=> '-e use constant j => qq{junk}; print j',
	      prog	=> 'use constant j => qq{junk}; print j',
	      bcopts	=> '-exec',
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <0> enter 
# 2  <;> nextstate(main 71 -e:1) v:>,<,%,{
# 3  <0> pushmark s
# 4  <$> const[PV "junk"] s*      < 5.017002
# 4  <$> const[PV "junk"] s*/FOLD >=5.017002
# 5  <@> print vK
# 6  <@> leave[1 ref] vKP/REFC
EOT_EOT
# 1  <0> enter 
# 2  <;> nextstate(main 71 -e:1) v:>,<,%,{
# 3  <0> pushmark s
# 4  <$> const(PV "junk") s*      < 5.017002
# 4  <$> const(PV "junk") s*/FOLD >=5.017002
# 5  <@> print vK
# 6  <@> leave[1 ref] vKP/REFC
EONT_EONT

pass("rpeep - return \$x at end of sub");

checkOptree ( name	=> '-exec sub { return 1 }',
	      code	=> sub { return 1 },
	      bcopts	=> '-exec',
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 1 -e:1) v:>,<,%
# 2  <$> const[IV 1] s
# 3  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 1 -e:1) v:>,<,%
# 2  <$> const(IV 1) s
# 3  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

pass("rpeep - if ($a || $b)");

checkOptree ( name	=> 'if ($a || $b) { } return 1',
	      code	=> 'if ($a || $b) { } return 1',
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 997 (eval 15):1) v
# 2  <#> gvsv[*a] s
# 3  <|> or(other->4) sK/1
# 4      <#> gvsv[*b] s
# 5      <|> and(other->6) vK/1
# 6  <0> stub v
# 7  <;> nextstate(main 997 (eval 15):1) v
# 8  <$> const[IV 1] s
# 9  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 997 (eval 15):1) v
# 2  <$> gvsv(*a) s
# 3  <|> or(other->4) sK/1
# 4      <$> gvsv(*b) s
# 5      <|> and(other->6) vK/1
# 6  <0> stub v
# 7  <;> nextstate(main 3 (eval 3):1) v
# 8  <$> const(IV 1) s
# 9  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

pass("rpeep - unless ($a && $b)");

checkOptree ( name	=> 'unless ($a && $b) { } return 1',
	      code	=> 'unless ($a && $b) { } return 1',
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 997 (eval 15):1) v
# 2  <#> gvsv[*a] s
# 3  <|> and(other->4) sK/1
# 4      <#> gvsv[*b] s
# 5      <|> or(other->6) vK/1
# 6  <0> stub v
# 7  <;> nextstate(main 997 (eval 15):1) v
# 8  <$> const[IV 1] s
# 9  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 997 (eval 15):1) v
# 2  <$> gvsv(*a) s
# 3  <|> and(other->4) sK/1
# 4      <$> gvsv(*b) s
# 5      <|> or(other->6) vK/1
# 6  <0> stub v
# 7  <;> nextstate(main 3 (eval 3):1) v
# 8  <$> const(IV 1) s
# 9  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

pass("rpeep - my $a; my @b; my %c; print 'f'");

checkOptree ( name	=> 'my $a; my @b; my %c; return 1',
	      code	=> 'my $a; my @b; my %c; return 1',
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 991 (eval 17):1) v
# 2  <0> padrange[$a:991,994; @b:992,994; %c:993,994] vM/LVINTRO,3
# 3  <;> nextstate(main 994 (eval 17):1) v:{
# 4  <$> const[IV 1] s
# 5  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 991 (eval 17):1) v
# 2  <0> padrange[$a:991,994; @b:992,994; %c:993,994] vM/LVINTRO,3
# 3  <;> nextstate(main 994 (eval 17):1) v:{
# 4  <$> const(IV 1) s
# 5  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

__END__

#######################################################################

checkOptree ( name	=> '-exec sub a { print (shift) ? "foo" : "bar" }',
	      code	=> sub { print (shift) ? "foo" : "bar" },
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
   insert threaded reference here
EOT_EOT
   insert non-threaded reference here
EONT_EONT

