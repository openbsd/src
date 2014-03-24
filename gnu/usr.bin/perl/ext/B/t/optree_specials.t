#!./perl

# This tests the B:: module(s) with CHECK, BEGIN, END and INIT blocks. The
# text excerpts below marked with "# " in front are the expected output. They
# are there twice, EOT for threading, and EONT for a non-threading Perl. The
# output is matched losely. If the match fails even though the "got" and
# "expected" output look exactly the same, then watch for trailing, invisible
# spaces.

BEGIN {
    unshift @INC, 't';
    require Config;
    if (($Config::Config{'extensions'} !~ /\bB\b/) ){
        print "1..0 # Skip -- Perl configured without B module\n";
        exit 0;
    }
}

# import checkOptree(), and %gOpts (containing test state)
use OptreeCheck;	# ALSO DOES @ARGV HANDLING !!!!!!
use Config;

plan tests => 15;

require_ok("B::Concise");

my $out = runperl(
    switches => ["-MO=Concise,BEGIN,CHECK,INIT,END,-exec"],
    prog => q{$a=$b && print q/foo/},
    stderr => 1 );

#print "out:$out\n";

my $src = q[our ($beg, $chk, $init, $end, $uc) = qq{'foo'}; BEGIN { $beg++ } CHECK { $chk++ } INIT { $init++ } END { $end++ } UNITCHECK {$uc++}];


checkOptree ( name	=> 'BEGIN',
	      bcopts	=> 'BEGIN',
	      prog	=> $src,
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# BEGIN 1:
# a  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->a
# 1        <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,x*,x&,x$,$ ->2
# 3        <1> require sK/1 ->4
# 2           <$> const[PV "strict.pm"] s/BARE ->3
# 4        <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,x*,x&,x$,$ ->5
# -        <@> lineseq K ->-
# -           <0> null ->5
# 9           <1> entersub[t1] KS*/TARG,2 ->a
# 5              <0> pushmark s ->6
# 6              <$> const[PV "strict"] sM ->7
# 7              <$> const[PV "refs"] sM ->8
# 8              <$> method_named[PV "unimport"] ->9
# BEGIN 2:
# k  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq K ->k
# b        <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,x*,x&,x$,$ ->c
# d        <1> require sK/1 ->e
# c           <$> const[PV "strict.pm"] s/BARE ->d
# e        <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,x*,x&,x$,$ ->f
# -        <@> lineseq K ->-
# -           <0> null ->f
# j           <1> entersub[t1] KS*/TARG,2 ->k
# f              <0> pushmark s ->g
# g              <$> const[PV "strict"] sM ->h
# h              <$> const[PV "refs"] sM ->i
# i              <$> method_named[PV "unimport"] ->j
# BEGIN 3:
# u  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->u
# l        <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,x*,x&,x$,$ ->m
# n        <1> require sK/1 ->o
# m           <$> const[PV "warnings.pm"] s/BARE ->n
# o        <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,x*,x&,x$,$ ->p
# -        <@> lineseq K ->-
# -           <0> null ->p
# t           <1> entersub[t1] KS*/TARG,2 ->u
# p              <0> pushmark s ->q
# q              <$> const[PV "warnings"] sM ->r
# r              <$> const[PV "qw"] sM ->s
# s              <$> method_named[PV "unimport"] ->t
# BEGIN 4:
# y  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->y
# v        <;> nextstate(main 2 -e:1) v:>,<,%,{ ->w
# x        <1> postinc[t3] sK/1 ->y
# -           <1> ex-rv2sv sKRM/1 ->x
# w              <#> gvsv[*beg] s ->x
EOT_EOT
# BEGIN 1:
# a  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->a
# 1        <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,x*,x&,x$,$ ->2
# 3        <1> require sK/1 ->4
# 2           <$> const(PV "strict.pm") s/BARE ->3
# 4        <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,x*,x&,x$,$ ->5
# -        <@> lineseq K ->-
# -           <0> null ->5
# 9           <1> entersub[t1] KS*/TARG,2 ->a
# 5              <0> pushmark s ->6
# 6              <$> const(PV "strict") sM ->7
# 7              <$> const(PV "refs") sM ->8
# 8              <$> method_named(PV "unimport") ->9
# BEGIN 2:
# k  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq K ->k
# b        <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,x*,x&,x$,$ ->c
# d        <1> require sK/1 ->e
# c           <$> const(PV "strict.pm") s/BARE ->d
# e        <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,x*,x&,x$,$ ->f
# -        <@> lineseq K ->-
# -           <0> null ->f
# j           <1> entersub[t1] KS*/TARG,2 ->k
# f              <0> pushmark s ->g
# g              <$> const(PV "strict") sM ->h
# h              <$> const(PV "refs") sM ->i
# i              <$> method_named(PV "unimport") ->j
# BEGIN 3:
# u  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->u
# l        <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,x*,x&,x$,$ ->m
# n        <1> require sK/1 ->o
# m           <$> const(PV "warnings.pm") s/BARE ->n
# o        <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,x*,x&,x$,$ ->p
# -        <@> lineseq K ->-
# -           <0> null ->p
# t           <1> entersub[t1] KS*/TARG,2 ->u
# p              <0> pushmark s ->q
# q              <$> const(PV "warnings") sM ->r
# r              <$> const(PV "qw") sM ->s
# s              <$> method_named(PV "unimport") ->t
# BEGIN 4:
# y  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->y
# v        <;> nextstate(main 2 -e:1) v:>,<,%,{ ->w
# x        <1> postinc[t2] sK/1 ->y
# -           <1> ex-rv2sv sKRM/1 ->x
# w              <$> gvsv(*beg) s ->x
EONT_EONT


checkOptree ( name	=> 'END',
	      bcopts	=> 'END',
	      prog	=> $src,
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# END 1:
# 4  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->4
# 1        <;> nextstate(main 5 -e:6) v:>,<,%,{ ->2
# 3        <1> postinc[t3] sK/1 ->4
# -           <1> ex-rv2sv sKRM/1 ->3
# 2              <#> gvsv[*end] s ->3
EOT_EOT
# END 1:
# 4  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->4
# 1        <;> nextstate(main 5 -e:6) v:>,<,%,{ ->2
# 3        <1> postinc[t2] sK/1 ->4
# -           <1> ex-rv2sv sKRM/1 ->3
# 2              <$> gvsv(*end) s ->3
EONT_EONT


checkOptree ( name	=> 'CHECK',
	      bcopts	=> 'CHECK',
	      prog	=> $src,
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# CHECK 1:
# 4  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->4
# 1        <;> nextstate(main 3 -e:4) v:>,<,%,{ ->2
# 3        <1> postinc[t3] sK/1 ->4
# -           <1> ex-rv2sv sKRM/1 ->3
# 2              <#> gvsv[*chk] s ->3
EOT_EOT
# CHECK 1:
# 4  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->4
# 1        <;> nextstate(main 3 -e:4) v:>,<,%,{ ->2
# 3        <1> postinc[t2] sK/1 ->4
# -           <1> ex-rv2sv sKRM/1 ->3
# 2              <$> gvsv(*chk) s ->3
EONT_EONT

checkOptree ( name	=> 'UNITCHECK',
	      bcopts=> 'UNITCHECK',
	      prog	=> $src,
	      strip_open_hints => 1,
	      expect=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# UNITCHECK 1:
# 4  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->4
# 1        <;> nextstate(main 3 -e:4) v:>,<,%,{ ->2
# 3        <1> postinc[t3] sK/1 ->4
# -           <1> ex-rv2sv sKRM/1 ->3
# 2              <#> gvsv[*uc] s ->3
EOT_EOT
# UNITCHECK 1:
# 4  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->4
# 1        <;> nextstate(main 3 -e:4) v:>,<,%,{ ->2
# 3        <1> postinc[t2] sK/1 ->4
# -           <1> ex-rv2sv sKRM/1 ->3
# 2              <$> gvsv(*uc) s ->3
EONT_EONT

checkOptree ( name	=> 'INIT',
	      bcopts	=> 'INIT',
	      #todo	=> 'get working',
	      prog	=> $src,
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# INIT 1:
# 4  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->4
# 1        <;> nextstate(main 4 -e:5) v:>,<,%,{ ->2
# 3        <1> postinc[t3] sK/1 ->4
# -           <1> ex-rv2sv sKRM/1 ->3
# 2              <#> gvsv[*init] s ->3
EOT_EOT
# INIT 1:
# 4  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->4
# 1        <;> nextstate(main 4 -e:5) v:>,<,%,{ ->2
# 3        <1> postinc[t2] sK/1 ->4
# -           <1> ex-rv2sv sKRM/1 ->3
# 2              <$> gvsv(*init) s ->3
EONT_EONT


checkOptree ( name	=> 'all of BEGIN END INIT CHECK UNITCHECK -exec',
	      bcopts	=> [qw/ BEGIN END INIT CHECK UNITCHECK -exec /],
	      prog	=> $src,
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# BEGIN 1:
# 1  <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,x*,x&,x$,$
# 2  <$> const[PV "strict.pm"] s/BARE
# 3  <1> require sK/1
# 4  <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,x*,x&,x$,$
# 5  <0> pushmark s
# 6  <$> const[PV "strict"] sM
# 7  <$> const[PV "refs"] sM
# 8  <$> method_named[PV "unimport"] 
# 9  <1> entersub[t1] KS*/TARG,2
# a  <1> leavesub[1 ref] K/REFC,1
# BEGIN 2:
# b  <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,x*,x&,x$,$
# c  <$> const[PV "strict.pm"] s/BARE
# d  <1> require sK/1
# e  <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,x*,x&,x$,$
# f  <0> pushmark s
# g  <$> const[PV "strict"] sM
# h  <$> const[PV "refs"] sM
# i  <$> method_named[PV "unimport"] 
# j  <1> entersub[t1] KS*/TARG,2
# k  <1> leavesub[1 ref] K/REFC,1
# BEGIN 3:
# l  <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,x*,x&,x$,$
# m  <$> const[PV "warnings.pm"] s/BARE
# n  <1> require sK/1
# o  <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,x*,x&,x$,$
# p  <0> pushmark s
# q  <$> const[PV "warnings"] sM
# r  <$> const[PV "qw"] sM
# s  <$> method_named[PV "unimport"] 
# t  <1> entersub[t1] KS*/TARG,2
# u  <1> leavesub[1 ref] K/REFC,1
# BEGIN 4:
# v  <;> nextstate(main 2 -e:1) v:>,<,%,{
# w  <#> gvsv[*beg] s
# x  <1> postinc[t3] sK/1
# y  <1> leavesub[1 ref] K/REFC,1
# END 1:
# z  <;> nextstate(main 5 -e:1) v:>,<,%,{
# 10 <#> gvsv[*end] s
# 11 <1> postinc[t3] sK/1
# 12 <1> leavesub[1 ref] K/REFC,1
# INIT 1:
# 13 <;> nextstate(main 4 -e:1) v:>,<,%,{
# 14 <#> gvsv[*init] s
# 15 <1> postinc[t3] sK/1
# 16 <1> leavesub[1 ref] K/REFC,1
# CHECK 1:
# 17 <;> nextstate(main 3 -e:1) v:>,<,%,{
# 18 <#> gvsv[*chk] s
# 19 <1> postinc[t3] sK/1
# 1a <1> leavesub[1 ref] K/REFC,1
# UNITCHECK 1:
# 1b <;> nextstate(main 6 -e:1) v:>,<,%,{
# 1c <#> gvsv[*uc] s
# 1d <1> postinc[t3] sK/1
# 1e <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# BEGIN 1:
# 1  <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,x*,x&,x$,$
# 2  <$> const(PV "strict.pm") s/BARE
# 3  <1> require sK/1
# 4  <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,x*,x&,x$,$
# 5  <0> pushmark s
# 6  <$> const(PV "strict") sM
# 7  <$> const(PV "refs") sM
# 8  <$> method_named(PV "unimport") 
# 9  <1> entersub[t1] KS*/TARG,2
# a  <1> leavesub[1 ref] K/REFC,1
# BEGIN 2:
# b  <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,x*,x&,x$,$
# c  <$> const(PV "strict.pm") s/BARE
# d  <1> require sK/1
# e  <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,x*,x&,x$,$
# f  <0> pushmark s
# g  <$> const(PV "strict") sM
# h  <$> const(PV "refs") sM
# i  <$> method_named(PV "unimport") 
# j  <1> entersub[t1] KS*/TARG,2
# k  <1> leavesub[1 ref] K/REFC,1
# BEGIN 3:
# l  <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,x*,x&,x$,$
# m  <$> const(PV "warnings.pm") s/BARE
# n  <1> require sK/1
# o  <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,x*,x&,x$,$
# p  <0> pushmark s
# q  <$> const(PV "warnings") sM
# r  <$> const(PV "qw") sM
# s  <$> method_named(PV "unimport") 
# t  <1> entersub[t1] KS*/TARG,2
# u  <1> leavesub[1 ref] K/REFC,1
# BEGIN 4:
# v  <;> nextstate(main 2 -e:1) v:>,<,%,{
# w  <$> gvsv(*beg) s
# x  <1> postinc[t2] sK/1
# y  <1> leavesub[1 ref] K/REFC,1
# END 1:
# z  <;> nextstate(main 5 -e:1) v:>,<,%,{
# 10 <$> gvsv(*end) s
# 11 <1> postinc[t2] sK/1
# 12 <1> leavesub[1 ref] K/REFC,1
# INIT 1:
# 13 <;> nextstate(main 4 -e:1) v:>,<,%,{
# 14 <$> gvsv(*init) s
# 15 <1> postinc[t2] sK/1
# 16 <1> leavesub[1 ref] K/REFC,1
# CHECK 1:
# 17 <;> nextstate(main 3 -e:1) v:>,<,%,{
# 18 <$> gvsv(*chk) s
# 19 <1> postinc[t2] sK/1
# 1a <1> leavesub[1 ref] K/REFC,1
# UNITCHECK 1:
# 1b <;> nextstate(main 6 -e:1) v:>,<,%,{
# 1c <$> gvsv(*uc) s
# 1d <1> postinc[t2] sK/1
# 1e <1> leavesub[1 ref] K/REFC,1
EONT_EONT


# perl "-I../lib" -MO=Concise,BEGIN,CHECK,INIT,END,-exec -e '$a=$b && print q/foo/'



checkOptree ( name	=> 'regression test for patch 25352',
	      bcopts	=> [qw/ BEGIN END INIT CHECK -exec /],
	      prog	=> 'print q/foo/',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# BEGIN 1:
# 1  <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,x*,x&,x$,$
# 2  <$> const[PV "strict.pm"] s/BARE
# 3  <1> require sK/1
# 4  <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,x*,x&,x$,$
# 5  <0> pushmark s
# 6  <$> const[PV "strict"] sM
# 7  <$> const[PV "refs"] sM
# 8  <$> method_named[PV "unimport"] 
# 9  <1> entersub[t1] KS*/TARG,2
# a  <1> leavesub[1 ref] K/REFC,1
# BEGIN 2:
# b  <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,x*,x&,x$,$
# c  <$> const[PV "strict.pm"] s/BARE
# d  <1> require sK/1
# e  <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,x*,x&,x$,$
# f  <0> pushmark s
# g  <$> const[PV "strict"] sM
# h  <$> const[PV "refs"] sM
# i  <$> method_named[PV "unimport"] 
# j  <1> entersub[t1] KS*/TARG,2
# k  <1> leavesub[1 ref] K/REFC,1
# BEGIN 3:
# l  <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,x*,x&,x$,$
# m  <$> const[PV "warnings.pm"] s/BARE
# n  <1> require sK/1
# o  <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,x*,x&,x$,$
# p  <0> pushmark s
# q  <$> const[PV "warnings"] sM
# r  <$> const[PV "qw"] sM
# s  <$> method_named[PV "unimport"] 
# t  <1> entersub[t1] KS*/TARG,2
# u  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# BEGIN 1:
# 1  <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,x*,x&,x$,$
# 2  <$> const(PV "strict.pm") s/BARE
# 3  <1> require sK/1
# 4  <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,x*,x&,x$,$
# 5  <0> pushmark s
# 6  <$> const(PV "strict") sM
# 7  <$> const(PV "refs") sM
# 8  <$> method_named(PV "unimport") 
# 9  <1> entersub[t1] KS*/TARG,2
# a  <1> leavesub[1 ref] K/REFC,1
# BEGIN 2:
# b  <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,x*,x&,x$,$
# c  <$> const(PV "strict.pm") s/BARE
# d  <1> require sK/1
# e  <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,x*,x&,x$,$
# f  <0> pushmark s
# g  <$> const(PV "strict") sM
# h  <$> const(PV "refs") sM
# i  <$> method_named(PV "unimport") 
# j  <1> entersub[t1] KS*/TARG,2
# k  <1> leavesub[1 ref] K/REFC,1
# BEGIN 3:
# l  <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,x*,x&,x$,$
# m  <$> const(PV "warnings.pm") s/BARE
# n  <1> require sK/1
# o  <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,x*,x&,x$,$
# p  <0> pushmark s
# q  <$> const(PV "warnings") sM
# r  <$> const(PV "qw") sM
# s  <$> method_named(PV "unimport") 
# t  <1> entersub[t1] KS*/TARG,2
# u  <1> leavesub[1 ref] K/REFC,1
EONT_EONT
