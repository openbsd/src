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
    # require 'test.pl'; # now done by OptreeCheck
}
use OptreeCheck;
use Config;
plan tests	=> 20;
SKIP: {
    skip "no perlio in this build", 20 unless $Config::Config{useperlio};

pass("GENERAL OPTREE EXAMPLES");

pass("IF,THEN,ELSE, ?:");

checkOptree ( name	=> '-basic sub {if shift print then,else}',
	      bcopts	=> '-basic',
	      code	=> sub { if (shift) { print "then" }
				 else       { print "else" }
			     },
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 9  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->9
# 1        <;> nextstate(main 426 optree.t:16) v ->2
# -        <1> null K/1 ->-
# 5           <|> cond_expr(other->6) K/1 ->a
# 4              <1> shift sK/1 ->5
# 3                 <1> rv2av[t2] sKRM/1 ->4
# 2                    <#> gv[*_] s ->3
# -              <@> scope K ->-
# -                 <0> ex-nextstate v ->6
# 8                 <@> print sK ->9
# 6                    <0> pushmark s ->7
# 7                    <$> const[PV "then"] s ->8
# f              <@> leave KP ->9
# a                 <0> enter ->b
# b                 <;> nextstate(main 424 optree.t:17) v ->c
# e                 <@> print sK ->f
# c                    <0> pushmark s ->d
# d                    <$> const[PV "else"] s ->e
EOT_EOT
# 9  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->9
# 1        <;> nextstate(main 427 optree_samples.t:18) v ->2
# -        <1> null K/1 ->-
# 5           <|> cond_expr(other->6) K/1 ->a
# 4              <1> shift sK/1 ->5
# 3                 <1> rv2av[t1] sKRM/1 ->4
# 2                    <$> gv(*_) s ->3
# -              <@> scope K ->-
# -                 <0> ex-nextstate v ->6
# 8                 <@> print sK ->9
# 6                    <0> pushmark s ->7
# 7                    <$> const(PV "then") s ->8
# f              <@> leave KP ->9
# a                 <0> enter ->b
# b                 <;> nextstate(main 425 optree_samples.t:19) v ->c
# e                 <@> print sK ->f
# c                    <0> pushmark s ->d
# d                    <$> const(PV "else") s ->e
EONT_EONT

checkOptree ( name	=> '-basic (see above, with my $a = shift)',
	      bcopts	=> '-basic',
	      code	=> sub { my $a = shift;
				 if ($a) { print "foo" }
				 else    { print "bar" }
			     },
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# d  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->d
# 1        <;> nextstate(main 431 optree.t:68) v ->2
# 6        <2> sassign vKS/2 ->7
# 4           <1> shift sK/1 ->5
# 3              <1> rv2av[t3] sKRM/1 ->4
# 2                 <#> gv[*_] s ->3
# 5           <0> padsv[$a:431,435] sRM*/LVINTRO ->6
# 7        <;> nextstate(main 435 optree.t:69) v ->8
# -        <1> null K/1 ->-
# 9           <|> cond_expr(other->a) K/1 ->e
# 8              <0> padsv[$a:431,435] s ->9
# -              <@> scope K ->-
# -                 <0> ex-nextstate v ->a
# c                 <@> print sK ->d
# a                    <0> pushmark s ->b
# b                    <$> const[PV "foo"] s ->c
# j              <@> leave KP ->d
# e                 <0> enter ->f
# f                 <;> nextstate(main 433 optree.t:70) v ->g
# i                 <@> print sK ->j
# g                    <0> pushmark s ->h
# h                    <$> const[PV "bar"] s ->i
EOT_EOT
# d  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->d
# 1        <;> nextstate(main 428 optree_samples.t:48) v ->2
# 6        <2> sassign vKS/2 ->7
# 4           <1> shift sK/1 ->5
# 3              <1> rv2av[t2] sKRM/1 ->4
# 2                 <$> gv(*_) s ->3
# 5           <0> padsv[$a:428,432] sRM*/LVINTRO ->6
# 7        <;> nextstate(main 432 optree_samples.t:49) v ->8
# -        <1> null K/1 ->-
# 9           <|> cond_expr(other->a) K/1 ->e
# 8              <0> padsv[$a:428,432] s ->9
# -              <@> scope K ->-
# -                 <0> ex-nextstate v ->a
# c                 <@> print sK ->d
# a                    <0> pushmark s ->b
# b                    <$> const(PV "foo") s ->c
# j              <@> leave KP ->d
# e                 <0> enter ->f
# f                 <;> nextstate(main 430 optree_samples.t:50) v ->g
# i                 <@> print sK ->j
# g                    <0> pushmark s ->h
# h                    <$> const(PV "bar") s ->i
EONT_EONT

checkOptree ( name	=> '-exec sub {if shift print then,else}',
	      bcopts	=> '-exec',
	      code	=> sub { if (shift) { print "then" }
				 else       { print "else" }
			     },
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 426 optree.t:16) v
# 2  <#> gv[*_] s
# 3  <1> rv2av[t2] sKRM/1
# 4  <1> shift sK/1
# 5  <|> cond_expr(other->6) K/1
# 6      <0> pushmark s
# 7      <$> const[PV "then"] s
# 8      <@> print sK
#            goto 9
# a  <0> enter 
# b  <;> nextstate(main 424 optree.t:17) v
# c  <0> pushmark s
# d  <$> const[PV "else"] s
# e  <@> print sK
# f  <@> leave KP
# 9  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 436 optree_samples.t:123) v
# 2  <$> gv(*_) s
# 3  <1> rv2av[t1] sKRM/1
# 4  <1> shift sK/1
# 5  <|> cond_expr(other->6) K/1
# 6      <0> pushmark s
# 7      <$> const(PV "then") s
# 8      <@> print sK
#            goto 9
# a  <0> enter 
# b  <;> nextstate(main 434 optree_samples.t:124) v
# c  <0> pushmark s
# d  <$> const(PV "else") s
# e  <@> print sK
# f  <@> leave KP
# 9  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

checkOptree ( name	=> '-exec (see above, with my $a = shift)',
	      bcopts	=> '-exec',
	      code	=> sub { my $a = shift;
				 if ($a) { print "foo" }
				 else    { print "bar" }
			     },
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 423 optree.t:16) v
# 2  <#> gv[*_] s
# 3  <1> rv2av[t3] sKRM/1
# 4  <1> shift sK/1
# 5  <0> padsv[$a:423,427] sRM*/LVINTRO
# 6  <2> sassign vKS/2
# 7  <;> nextstate(main 427 optree.t:17) v
# 8  <0> padsv[$a:423,427] s
# 9  <|> cond_expr(other->a) K/1
# a      <0> pushmark s
# b      <$> const[PV "foo"] s
# c      <@> print sK
#            goto d
# e  <0> enter 
# f  <;> nextstate(main 425 optree.t:18) v
# g  <0> pushmark s
# h  <$> const[PV "bar"] s
# i  <@> print sK
# j  <@> leave KP
# d  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 437 optree_samples.t:112) v
# 2  <$> gv(*_) s
# 3  <1> rv2av[t2] sKRM/1
# 4  <1> shift sK/1
# 5  <0> padsv[$a:437,441] sRM*/LVINTRO
# 6  <2> sassign vKS/2
# 7  <;> nextstate(main 441 optree_samples.t:113) v
# 8  <0> padsv[$a:437,441] s
# 9  <|> cond_expr(other->a) K/1
# a      <0> pushmark s
# b      <$> const(PV "foo") s
# c      <@> print sK
#            goto d
# e  <0> enter 
# f  <;> nextstate(main 439 optree_samples.t:114) v
# g  <0> pushmark s
# h  <$> const(PV "bar") s
# i  <@> print sK
# j  <@> leave KP
# d  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

checkOptree ( name	=> '-exec sub { print (shift) ? "foo" : "bar" }',
	      code	=> sub { print (shift) ? "foo" : "bar" },
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 428 optree.t:31) v
# 2  <0> pushmark s
# 3  <#> gv[*_] s
# 4  <1> rv2av[t2] sKRM/1
# 5  <1> shift sK/1
# 6  <@> print sK
# 7  <|> cond_expr(other->8) K/1
# 8      <$> const[PV "foo"] s
#            goto 9
# a  <$> const[PV "bar"] s
# 9  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 442 optree_samples.t:144) v
# 2  <0> pushmark s
# 3  <$> gv(*_) s
# 4  <1> rv2av[t1] sKRM/1
# 5  <1> shift sK/1
# 6  <@> print sK
# 7  <|> cond_expr(other->8) K/1
# 8      <$> const(PV "foo") s
#            goto 9
# a  <$> const(PV "bar") s
# 9  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

pass ("FOREACH");

checkOptree ( name	=> '-exec sub { foreach (1..10) {print "foo $_"} }',
	      code	=> sub { foreach (1..10) {print "foo $_"} },
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 443 optree.t:158) v
# 2  <0> pushmark s
# 3  <$> const[IV 1] s
# 4  <$> const[IV 10] s
# 5  <#> gv[*_] s
# 6  <{> enteriter(next->d last->g redo->7) lKS
# e  <0> iter s
# f  <|> and(other->7) K/1
# 7      <;> nextstate(main 442 optree.t:158) v
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
# 1  <;> nextstate(main 444 optree_samples.t:182) v
# 2  <0> pushmark s
# 3  <$> const(IV 1) s
# 4  <$> const(IV 10) s
# 5  <$> gv(*_) s
# 6  <{> enteriter(next->d last->g redo->7) lKS
# e  <0> iter s
# f  <|> and(other->7) K/1
# 7      <;> nextstate(main 443 optree_samples.t:182) v
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
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# h  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->h
# 1        <;> nextstate(main 445 optree.t:167) v ->2
# 2        <;> nextstate(main 445 optree.t:167) v ->3
# g        <2> leaveloop K/2 ->h
# 7           <{> enteriter(next->d last->g redo->8) lKS ->e
# -              <0> ex-pushmark s ->3
# -              <1> ex-list lK ->6
# 3                 <0> pushmark s ->4
# 4                 <$> const[IV 1] s ->5
# 5                 <$> const[IV 10] s ->6
# 6              <#> gv[*_] s ->7
# -           <1> null K/1 ->g
# f              <|> and(other->8) K/1 ->g
# e                 <0> iter s ->f
# -                 <@> lineseq sK ->-
# c                    <@> print vK ->d
# 8                       <0> pushmark s ->9
# -                       <1> ex-stringify sK/1 ->c
# -                          <0> ex-pushmark s ->9
# b                          <2> concat[t2] sK/2 ->c
# 9                             <$> const[PV "foo "] s ->a
# -                             <1> ex-rv2sv sK/1 ->b
# a                                <#> gvsv[*_] s ->b
# d                    <0> unstack s ->e
EOT_EOT
# h  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->h
# 1        <;> nextstate(main 446 optree_samples.t:192) v ->2
# 2        <;> nextstate(main 446 optree_samples.t:192) v ->3
# g        <2> leaveloop K/2 ->h
# 7           <{> enteriter(next->d last->g redo->8) lKS ->e
# -              <0> ex-pushmark s ->3
# -              <1> ex-list lK ->6
# 3                 <0> pushmark s ->4
# 4                 <$> const(IV 1) s ->5
# 5                 <$> const(IV 10) s ->6
# 6              <$> gv(*_) s ->7
# -           <1> null K/1 ->g
# f              <|> and(other->8) K/1 ->g
# e                 <0> iter s ->f
# -                 <@> lineseq sK ->-
# c                    <@> print vK ->d
# 8                       <0> pushmark s ->9
# -                       <1> ex-stringify sK/1 ->c
# -                          <0> ex-pushmark s ->9
# b                          <2> concat[t1] sK/2 ->c
# 9                             <$> const(PV "foo ") s ->a
# -                             <1> ex-rv2sv sK/1 ->b
# a                                <$> gvsv(*_) s ->b
# d                    <0> unstack s ->e
EONT_EONT

checkOptree ( name	=> '-exec -e foreach (1..10) {print qq{foo $_}}',
	      prog	=> 'foreach (1..10) {print qq{foo $_}}',
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <0> enter 
# 2  <;> nextstate(main 2 -e:1) v
# 3  <0> pushmark s
# 4  <$> const[IV 1] s
# 5  <$> const[IV 10] s
# 6  <#> gv[*_] s
# 7  <{> enteriter(next->e last->h redo->8) lKS
# f  <0> iter s
# g  <|> and(other->8) vK/1
# 8      <;> nextstate(main 1 -e:1) v
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
# 2  <;> nextstate(main 2 -e:1) v
# 3  <0> pushmark s
# 4  <$> const(IV 1) s
# 5  <$> const(IV 10) s
# 6  <$> gv(*_) s
# 7  <{> enteriter(next->e last->h redo->8) lKS
# f  <0> iter s
# g  <|> and(other->8) vK/1
# 8      <;> nextstate(main 1 -e:1) v
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
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 445 optree.t:167) v
# 2  <;> nextstate(main 445 optree.t:167) v
# 3  <0> pushmark s
# 4  <$> const[IV 1] s
# 5  <$> const[IV 10] s
# 6  <#> gv[*_] s
# 7  <{> enteriter(next->d last->g redo->8) lKS
# e  <0> iter s
# f  <|> and(other->8) K/1
# 8      <0> pushmark s
# 9      <$> const[PV "foo "] s
# a      <#> gvsv[*_] s
# b      <2> concat[t2] sK/2
# c      <@> print vK
# d      <0> unstack s
#            goto e
# g  <2> leaveloop K/2
# h  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 447 optree_samples.t:252) v
# 2  <;> nextstate(main 447 optree_samples.t:252) v
# 3  <0> pushmark s
# 4  <$> const(IV 1) s
# 5  <$> const(IV 10) s
# 6  <$> gv(*_) s
# 7  <{> enteriter(next->d last->g redo->8) lKS
# e  <0> iter s
# f  <|> and(other->8) K/1
# 8      <0> pushmark s
# 9      <$> const(PV "foo ") s
# a      <$> gvsv(*_) s
# b      <2> concat[t1] sK/2
# c      <@> print vK
# d      <0> unstack s
#            goto e
# g  <2> leaveloop K/2
# h  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

pass("GREP: SAMPLES FROM PERLDOC -F GREP");

checkOptree ( name	=> '@foo = grep(!/^\#/, @bar)',
	      code	=> '@foo = grep(!/^\#/, @bar)',
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 496 (eval 20):1) v
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
# d  <2> aassign[t6] KS/COMMON
# e  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 496 (eval 20):1) v
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
# d  <2> aassign[t4] KS/COMMON
# e  <1> leavesub[1 ref] K/REFC,1
EONT_EONT


pass("MAP: SAMPLES FROM PERLDOC -F MAP");

checkOptree ( name	=> '%h = map { getkey($_) => $_ } @a',
	      code	=> '%h = map { getkey($_) => $_ } @a',
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 501 (eval 22):1) v
# 2  <0> pushmark s
# 3  <0> pushmark s
# 4  <#> gv[*a] s
# 5  <1> rv2av[t8] lKM/1
# 6  <@> mapstart lK*
# 7  <|> mapwhile(other->8)[t9] lK
# 8      <0> enter l
# 9      <;> nextstate(main 500 (eval 22):1) v
# a      <0> pushmark s
# b      <0> pushmark s
# c      <#> gvsv[*_] s
# d      <#> gv[*getkey] s/EARLYCV
# e      <1> entersub[t5] lKS/TARG,1
# f      <#> gvsv[*_] s
# g      <@> list lK
# h      <@> leave lKP
#            goto 7
# i  <0> pushmark s
# j  <#> gv[*h] s
# k  <1> rv2hv[t2] lKRM*/1
# l  <2> aassign[t10] KS/COMMON
# m  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 501 (eval 22):1) v
# 2  <0> pushmark s
# 3  <0> pushmark s
# 4  <$> gv(*a) s
# 5  <1> rv2av[t3] lKM/1
# 6  <@> mapstart lK*
# 7  <|> mapwhile(other->8)[t4] lK
# 8      <0> enter l
# 9      <;> nextstate(main 500 (eval 22):1) v
# a      <0> pushmark s
# b      <0> pushmark s
# c      <$> gvsv(*_) s
# d      <$> gv(*getkey) s/EARLYCV
# e      <1> entersub[t2] lKS/TARG,1
# f      <$> gvsv(*_) s
# g      <@> list lK
# h      <@> leave lKP
#            goto 7
# i  <0> pushmark s
# j  <$> gv(*h) s
# k  <1> rv2hv[t1] lKRM*/1
# l  <2> aassign[t5] KS/COMMON
# m  <1> leavesub[1 ref] K/REFC,1
EONT_EONT

checkOptree ( name	=> '%h=(); for $_(@a){$h{getkey($_)} = $_}',
	      code	=> '%h=(); for $_(@a){$h{getkey($_)} = $_}',
	      bcopts	=> '-exec',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <;> nextstate(main 505 (eval 24):1) v
# 2  <0> pushmark s
# 3  <0> pushmark s
# 4  <#> gv[*h] s
# 5  <1> rv2hv[t2] lKRM*/1
# 6  <2> aassign[t3] vKS
# 7  <;> nextstate(main 506 (eval 24):1) v
# 8  <0> pushmark sM
# 9  <#> gv[*a] s
# a  <1> rv2av[t6] sKRM/1
# b  <#> gv[*_] s
# c  <1> rv2gv sKRM/1
# d  <{> enteriter(next->o last->r redo->e) lKS
# p  <0> iter s
# q  <|> and(other->e) K/1
# e      <;> nextstate(main 505 (eval 24):1) v
# f      <#> gvsv[*_] s
# g      <#> gv[*h] s
# h      <1> rv2hv sKR/1
# i      <0> pushmark s
# j      <#> gvsv[*_] s
# k      <#> gv[*getkey] s/EARLYCV
# l      <1> entersub[t10] sKS/TARG,1
# m      <2> helem sKRM*/2
# n      <2> sassign vKS/2
# o      <0> unstack s
#            goto p
# r  <2> leaveloop K/2
# s  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# 1  <;> nextstate(main 505 (eval 24):1) v
# 2  <0> pushmark s
# 3  <0> pushmark s
# 4  <$> gv(*h) s
# 5  <1> rv2hv[t1] lKRM*/1
# 6  <2> aassign[t2] vKS
# 7  <;> nextstate(main 506 (eval 24):1) v
# 8  <0> pushmark sM
# 9  <$> gv(*a) s
# a  <1> rv2av[t3] sKRM/1
# b  <$> gv(*_) s
# c  <1> rv2gv sKRM/1
# d  <{> enteriter(next->o last->r redo->e) lKS
# p  <0> iter s
# q  <|> and(other->e) K/1
# e      <;> nextstate(main 505 (eval 24):1) v
# f      <$> gvsv(*_) s
# g      <$> gv(*h) s
# h      <1> rv2hv sKR/1
# i      <0> pushmark s
# j      <$> gvsv(*_) s
# k      <$> gv(*getkey) s/EARLYCV
# l      <1> entersub[t4] sKS/TARG,1
# m      <2> helem sKRM*/2
# n      <2> sassign vKS/2
# o      <0> unstack s
#            goto p
# r  <2> leaveloop K/2
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
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# 1  <0> enter 
# 2  <;> nextstate(main 71 -e:1) v
# 3  <0> pushmark s
# 4  <$> const[PV "junk"] s
# 5  <@> print vK
# 6  <@> leave[1 ref] vKP/REFC
EOT_EOT
# 1  <0> enter 
# 2  <;> nextstate(main 71 -e:1) v
# 3  <0> pushmark s
# 4  <$> const(PV "junk") s
# 5  <@> print vK
# 6  <@> leave[1 ref] vKP/REFC
EONT_EONT

} # skip

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

