#!./perl

# This tests the B:: module(s) with CHECK, BEGIN, END and INIT blocks. The
# text excerpts below marked with "# " in front are the expected output. They
# are there twice, EOT for threading, and EONT for a non-threading Perl. The
# output is matched losely. If the match fails even though the "got" and
# "expected" output look exactly the same, then watch for trailing, invisible
# spaces.

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

# import checkOptree(), and %gOpts (containing test state)
use OptreeCheck;	# ALSO DOES @ARGV HANDLING !!!!!!
use Config;

plan tests => 7 + ($] > 5.009 ? 1 : 0);

require_ok("B::Concise");

my $out = runperl(
    switches => ["-MO=Concise,BEGIN,CHECK,INIT,END,-exec"],
    prog => q{$a=$b && print q/foo/},
    stderr => 1 );

#print "out:$out\n";

my $src = q[our ($beg, $chk, $init, $end, $uc) = qq{'foo'}; BEGIN { $beg++ } CHECK { $chk++ } INIT { $init++ } END { $end++ } UNITCHECK {$uc++}];


my @warnings_todo;
@warnings_todo = (todo =>
   "Change 23768 (Remove Carp from warnings.pm) alters expected output, not"
   . "propagated to 5.8.x")
    if $] < 5.009;

checkOptree ( name	=> 'BEGIN',
	      bcopts	=> 'BEGIN',
	      prog	=> $src,
	      @warnings_todo,
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# BEGIN 1:
# b  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->b
# 1        <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,$ ->2
# 3        <1> require sK/1 ->4
# 2           <$> const[PV "strict.pm"] s/BARE ->3
# 4        <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,$ ->5
# -        <@> lineseq K ->-
# 5           <;> nextstate(B::Concise -275 Concise.pm:356) :*,&,{,$ ->6
# a           <1> entersub[t1] KS*/TARG,2 ->b
# 6              <0> pushmark s ->7
# 7              <$> const[PV "strict"] sM ->8
# 8              <$> const[PV "refs"] sM ->9
# 9              <$> method_named[PV "unimport"] ->a
# BEGIN 2:
# m  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq K ->m
# c        <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,$ ->d
# e        <1> require sK/1 ->f
# d           <$> const[PV "strict.pm"] s/BARE ->e
# f        <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,$ ->g
# -        <@> lineseq K ->-
# g           <;> nextstate(B::Concise -265 Concise.pm:367) :*,&,$ ->h
# l           <1> entersub[t1] KS*/TARG,2 ->m
# h              <0> pushmark s ->i
# i              <$> const[PV "strict"] sM ->j
# j              <$> const[PV "refs"] sM ->k
# k              <$> method_named[PV "unimport"] ->l
# BEGIN 3:
# x  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->x
# n        <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,$ ->o
# p        <1> require sK/1 ->q
# o           <$> const[PV "warnings.pm"] s/BARE ->p
# q        <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,$ ->r
# -        <@> lineseq K ->-
# r           <;> nextstate(B::Concise -254 Concise.pm:386) :*,&,{,$ ->s
# w           <1> entersub[t1] KS*/TARG,2 ->x
# s              <0> pushmark s ->t
# t              <$> const[PV "warnings"] sM ->u
# u              <$> const[PV "qw"] sM ->v
# v              <$> method_named[PV "unimport"] ->w
# BEGIN 4:
# 11 <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->11
# y        <;> nextstate(main 2 -e:1) v:>,<,%,{ ->z
# 10       <1> postinc[t3] sK/1 ->11
# -           <1> ex-rv2sv sKRM/1 ->10
# z              <#> gvsv[*beg] s ->10
EOT_EOT
# BEGIN 1:
# b  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->b
# 1        <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,$ ->2
# 3        <1> require sK/1 ->4
# 2           <$> const(PV "strict.pm") s/BARE ->3
# 4        <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,$ ->5
# -        <@> lineseq K ->-
# 5           <;> nextstate(B::Concise -275 Concise.pm:356) :*,&,{,$ ->6
# a           <1> entersub[t1] KS*/TARG,2 ->b
# 6              <0> pushmark s ->7
# 7              <$> const(PV "strict") sM ->8
# 8              <$> const(PV "refs") sM ->9
# 9              <$> method_named(PV "unimport") ->a
# BEGIN 2:
# m  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq K ->m
# c        <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,$ ->d
# e        <1> require sK/1 ->f
# d           <$> const(PV "strict.pm") s/BARE ->e
# f        <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,$ ->g
# -        <@> lineseq K ->-
# g           <;> nextstate(B::Concise -265 Concise.pm:367) :*,&,$ ->h
# l           <1> entersub[t1] KS*/TARG,2 ->m
# h              <0> pushmark s ->i
# i              <$> const(PV "strict") sM ->j
# j              <$> const(PV "refs") sM ->k
# k              <$> method_named(PV "unimport") ->l
# BEGIN 3:
# x  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->x
# n        <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,$ ->o
# p        <1> require sK/1 ->q
# o           <$> const(PV "warnings.pm") s/BARE ->p
# q        <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,$ ->r
# -        <@> lineseq K ->-
# r           <;> nextstate(B::Concise -254 Concise.pm:386) :*,&,{,$ ->s
# w           <1> entersub[t1] KS*/TARG,2 ->x
# s              <0> pushmark s ->t
# t              <$> const(PV "warnings") sM ->u
# u              <$> const(PV "qw") sM ->v
# v              <$> method_named(PV "unimport") ->w
# BEGIN 4:
# 11 <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->11
# y        <;> nextstate(main 2 -e:1) v:>,<,%,{ ->z
# 10       <1> postinc[t2] sK/1 ->11
# -           <1> ex-rv2sv sKRM/1 ->10
# z              <$> gvsv(*beg) s ->10
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

if ($] >= 5.009) {
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
}

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
	      @warnings_todo,
	      strip_open_hints => 1,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# BEGIN 1:
# 1  <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,$
# 2  <$> const[PV "strict.pm"] s/BARE
# 3  <1> require sK/1
# 4  <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,$
# 5  <;> nextstate(B::Concise -275 Concise.pm:356) :*,&,{,$
# 6  <0> pushmark s
# 7  <$> const[PV "strict"] sM
# 8  <$> const[PV "refs"] sM
# 9  <$> method_named[PV "unimport"] 
# a  <1> entersub[t1] KS*/TARG,2
# b  <1> leavesub[1 ref] K/REFC,1
# BEGIN 2:
# c  <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,$
# d  <$> const[PV "strict.pm"] s/BARE
# e  <1> require sK/1
# f  <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,$
# g  <;> nextstate(B::Concise -265 Concise.pm:367) :*,&,$
# h  <0> pushmark s
# i  <$> const[PV "strict"] sM
# j  <$> const[PV "refs"] sM
# k  <$> method_named[PV "unimport"] 
# l  <1> entersub[t1] KS*/TARG,2
# m  <1> leavesub[1 ref] K/REFC,1
# BEGIN 3:
# n  <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,$
# o  <$> const[PV "warnings.pm"] s/BARE
# p  <1> require sK/1
# q  <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,$
# r  <;> nextstate(B::Concise -254 Concise.pm:386) :*,&,{,$
# s  <0> pushmark s
# t  <$> const[PV "warnings"] sM
# u  <$> const[PV "qw"] sM
# v  <$> method_named[PV "unimport"] 
# w  <1> entersub[t1] KS*/TARG,2
# x  <1> leavesub[1 ref] K/REFC,1
# BEGIN 4:
# y  <;> nextstate(main 2 -e:1) v:>,<,%,{
# z  <#> gvsv[*beg] s
# 10 <1> postinc[t3] sK/1
# 11 <1> leavesub[1 ref] K/REFC,1
# END 1:
# 12 <;> nextstate(main 5 -e:1) v:>,<,%,{
# 13 <#> gvsv[*end] s
# 14 <1> postinc[t3] sK/1
# 15 <1> leavesub[1 ref] K/REFC,1
# INIT 1:
# 16 <;> nextstate(main 4 -e:1) v:>,<,%,{
# 17 <#> gvsv[*init] s
# 18 <1> postinc[t3] sK/1
# 19 <1> leavesub[1 ref] K/REFC,1
# CHECK 1:
# 1a <;> nextstate(main 3 -e:1) v:>,<,%,{
# 1b <#> gvsv[*chk] s
# 1c <1> postinc[t3] sK/1
# 1d <1> leavesub[1 ref] K/REFC,1
# UNITCHECK 1:
# 1e <;> nextstate(main 6 -e:1) v:>,<,%,{
# 1f <#> gvsv[*uc] s
# 1g <1> postinc[t3] sK/1
# 1h <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# BEGIN 1:
# 1  <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,$
# 2  <$> const(PV "strict.pm") s/BARE
# 3  <1> require sK/1
# 4  <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,$
# 5  <;> nextstate(B::Concise -275 Concise.pm:356) :*,&,{,$
# 6  <0> pushmark s
# 7  <$> const(PV "strict") sM
# 8  <$> const(PV "refs") sM
# 9  <$> method_named(PV "unimport") 
# a  <1> entersub[t1] KS*/TARG,2
# b  <1> leavesub[1 ref] K/REFC,1
# BEGIN 2:
# c  <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,$
# d  <$> const(PV "strict.pm") s/BARE
# e  <1> require sK/1
# f  <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,$
# g  <;> nextstate(B::Concise -265 Concise.pm:367) :*,&,$
# h  <0> pushmark s
# i  <$> const(PV "strict") sM
# j  <$> const(PV "refs") sM
# k  <$> method_named(PV "unimport") 
# l  <1> entersub[t1] KS*/TARG,2
# m  <1> leavesub[1 ref] K/REFC,1
# BEGIN 3:
# n  <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,$
# o  <$> const(PV "warnings.pm") s/BARE
# p  <1> require sK/1
# q  <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,$
# r  <;> nextstate(B::Concise -254 Concise.pm:386) :*,&,{,$
# s  <0> pushmark s
# t  <$> const(PV "warnings") sM
# u  <$> const(PV "qw") sM
# v  <$> method_named(PV "unimport") 
# w  <1> entersub[t1] KS*/TARG,2
# x  <1> leavesub[1 ref] K/REFC,1
# BEGIN 4:
# y  <;> nextstate(main 2 -e:1) v:>,<,%,{
# z  <$> gvsv(*beg) s
# 10 <1> postinc[t2] sK/1
# 11 <1> leavesub[1 ref] K/REFC,1
# END 1:
# 12 <;> nextstate(main 5 -e:1) v:>,<,%,{
# 13 <$> gvsv(*end) s
# 14 <1> postinc[t2] sK/1
# 15 <1> leavesub[1 ref] K/REFC,1
# INIT 1:
# 16 <;> nextstate(main 4 -e:1) v:>,<,%,{
# 17 <$> gvsv(*init) s
# 18 <1> postinc[t2] sK/1
# 19 <1> leavesub[1 ref] K/REFC,1
# CHECK 1:
# 1a <;> nextstate(main 3 -e:1) v:>,<,%,{
# 1b <$> gvsv(*chk) s
# 1c <1> postinc[t2] sK/1
# 1d <1> leavesub[1 ref] K/REFC,1
# UNITCHECK 1:
# 1e <;> nextstate(main 6 -e:1) v:>,<,%,{
# 1f <$> gvsv(*uc) s
# 1g <1> postinc[t2] sK/1
# 1h <1> leavesub[1 ref] K/REFC,1
EONT_EONT


# perl "-I../lib" -MO=Concise,BEGIN,CHECK,INIT,END,-exec -e '$a=$b && print q/foo/'



checkOptree ( name	=> 'regression test for patch 25352',
	      bcopts	=> [qw/ BEGIN END INIT CHECK -exec /],
	      prog	=> 'print q/foo/',
	      @warnings_todo,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# BEGIN 1:
# 1  <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,$
# 2  <$> const[PV "strict.pm"] s/BARE
# 3  <1> require sK/1
# 4  <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,$
# 5  <;> nextstate(B::Concise -275 Concise.pm:356) :*,&,{,$
# 6  <0> pushmark s
# 7  <$> const[PV "strict"] sM
# 8  <$> const[PV "refs"] sM
# 9  <$> method_named[PV "unimport"] 
# a  <1> entersub[t1] KS*/TARG,2
# b  <1> leavesub[1 ref] K/REFC,1
# BEGIN 2:
# c  <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,$
# d  <$> const[PV "strict.pm"] s/BARE
# e  <1> require sK/1
# f  <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,$
# g  <;> nextstate(B::Concise -265 Concise.pm:367) :*,&,$
# h  <0> pushmark s
# i  <$> const[PV "strict"] sM
# j  <$> const[PV "refs"] sM
# k  <$> method_named[PV "unimport"] 
# l  <1> entersub[t1] KS*/TARG,2
# m  <1> leavesub[1 ref] K/REFC,1
# BEGIN 3:
# n  <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,$
# o  <$> const[PV "warnings.pm"] s/BARE
# p  <1> require sK/1
# q  <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,$
# r  <;> nextstate(B::Concise -254 Concise.pm:386) :*,&,{,$
# s  <0> pushmark s
# t  <$> const[PV "warnings"] sM
# u  <$> const[PV "qw"] sM
# v  <$> method_named[PV "unimport"] 
# w  <1> entersub[t1] KS*/TARG,2
# x  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# BEGIN 1:
# 1  <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,$
# 2  <$> const(PV "strict.pm") s/BARE
# 3  <1> require sK/1
# 4  <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,$
# 5  <;> nextstate(B::Concise -275 Concise.pm:356) :*,&,{,$
# 6  <0> pushmark s
# 7  <$> const(PV "strict") sM
# 8  <$> const(PV "refs") sM
# 9  <$> method_named(PV "unimport") 
# a  <1> entersub[t1] KS*/TARG,2
# b  <1> leavesub[1 ref] K/REFC,1
# BEGIN 2:
# c  <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,$
# d  <$> const(PV "strict.pm") s/BARE
# e  <1> require sK/1
# f  <;> nextstate(B::Concise -265 Concise.pm:367) v:*,&,$
# g  <;> nextstate(B::Concise -265 Concise.pm:367) :*,&,$
# h  <0> pushmark s
# i  <$> const(PV "strict") sM
# j  <$> const(PV "refs") sM
# k  <$> method_named(PV "unimport") 
# l  <1> entersub[t1] KS*/TARG,2
# m  <1> leavesub[1 ref] K/REFC,1
# BEGIN 3:
# n  <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,$
# o  <$> const(PV "warnings.pm") s/BARE
# p  <1> require sK/1
# q  <;> nextstate(B::Concise -254 Concise.pm:386) v:*,&,{,$
# r  <;> nextstate(B::Concise -254 Concise.pm:386) :*,&,{,$
# s  <0> pushmark s
# t  <$> const(PV "warnings") sM
# u  <$> const(PV "qw") sM
# v  <$> method_named(PV "unimport") 
# w  <1> entersub[t1] KS*/TARG,2
# x  <1> leavesub[1 ref] K/REFC,1
EONT_EONT
