#!./perl

# This tests the B:: module(s) with CHECK, BEGIN, END and INIT blocks. The
# text excerpts below marked with "# " in front are the expected output. They
# are there twice, EOT for threading, and EONT for a non-threading Perl. The
# output is matched losely. If the match fails even though the "got" and
# "expected" output look exactly the same, then watch for trailing, invisible
# spaces.
#
# Note that if this test is mysteriously failing smokes and is hard to
# reproduce, try running with LC_ALL=en_US.UTF-8 PERL_UNICODE="".
# This causes nextstate ops to have a bunch of extra hint info, which
# needs adding to the expected output (for both thraded and non-threaded
# versions)

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
# -     <@> lineseq KP ->7
# 1        <;> nextstate(B::Concise -1151 Concise.pm:116) v:*,&,{,x*,x&,x$,$ ->2
# 6        <2> sassign sKS/2 ->7
# 4           <1> srefgen sK/1 ->5
# -              <1> ex-list lKRM ->4
# 3                 <1> rv2gv sKRM/STRICT,1 ->4
# 2                    <#> gv[*STDOUT] s ->3
# -           <1> ex-rv2sv sKRM*/STRICT,1 ->6
# 5              <#> gvsv[*B::Concise::walkHandle] s ->6
# BEGIN 2:
# h  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq K ->h
# 8        <;> nextstate(B::Concise -1113 Concise.pm:181) v:*,&,x*,x&,x$,$ ->9
# a        <1> require sK/1 ->b
# 9           <$> const[PV "strict.pm"] s/BARE ->a
# -        <;> ex-nextstate(B::Concise -1113 Concise.pm:181) v:*,&,x*,x&,x$,$ ->b
# -        <@> lineseq K ->-
# b           <;> nextstate(B::Concise -1113 Concise.pm:181) :*,&,x*,x&,x$,$ ->c
# g           <1> entersub[t1] KRS*/TARG,STRICT ->h
# c              <0> pushmark s ->d
# d              <$> const[PV "strict"] sM ->e
# e              <$> const[PV "refs"] sM ->f
# f              <.> method_named[PV "unimport"] ->g
# BEGIN 3:
# r  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq K ->r
# i        <;> nextstate(B::Concise -1010 Concise.pm:303) v:*,&,x*,x&,x$,$ ->j
# k        <1> require sK/1 ->l
# j           <$> const[PV "strict.pm"] s/BARE ->k
# -        <;> ex-nextstate(B::Concise -1010 Concise.pm:303) v:*,&,x*,x&,x$,$ ->l
# -        <@> lineseq K ->-
# l           <;> nextstate(B::Concise -1010 Concise.pm:303) :*,&,x*,x&,x$,$ ->m
# q           <1> entersub[t1] KRS*/TARG,STRICT ->r
# m              <0> pushmark s ->n
# n              <$> const[PV "strict"] sM ->o
# o              <$> const[PV "refs"] sM ->p
# p              <.> method_named[PV "unimport"] ->q
# BEGIN 4:
# 11 <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->11
# s        <;> nextstate(B::Concise -963 Concise.pm:368) v:*,&,{,x*,x&,x$,$ ->t
# u        <1> require sK/1 ->v
# t           <$> const[PV "strict.pm"] s/BARE ->u
# -        <;> ex-nextstate(B::Concise -963 Concise.pm:368) v:*,&,{,x*,x&,x$,$ ->v
# -        <@> lineseq K ->-
# v           <;> nextstate(B::Concise -963 Concise.pm:368) :*,&,{,x*,x&,x$,$ ->w
# 10          <1> entersub[t1] KRS*/TARG,STRICT ->11
# w              <0> pushmark s ->x
# x              <$> const[PV "strict"] sM ->y
# y              <$> const[PV "refs"] sM ->z
# z              <.> method_named[PV "unimport"] ->10
# BEGIN 5:
# 1b <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq K ->1b
# 12       <;> nextstate(B::Concise -938 Concise.pm:388) v:*,&,x*,x&,x$,$ ->13
# 14       <1> require sK/1 ->15
# 13          <$> const[PV "strict.pm"] s/BARE ->14
# -        <;> ex-nextstate(B::Concise -938 Concise.pm:388) v:*,&,x*,x&,x$,$ ->15
# -        <@> lineseq K ->-
# 15          <;> nextstate(B::Concise -938 Concise.pm:388) :*,&,x*,x&,x$,$ ->16
# 1a          <1> entersub[t1] KRS*/TARG,STRICT ->1b
# 16             <0> pushmark s ->17
# 17             <$> const[PV "strict"] sM ->18
# 18             <$> const[PV "refs"] sM ->19
# 19             <.> method_named[PV "unimport"] ->1a
# BEGIN 6:
# 1l <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->1l
# 1c       <;> nextstate(B::Concise -924 Concise.pm:408) v:*,&,{,x*,x&,x$,$ ->1d
# 1e       <1> require sK/1 ->1f
# 1d          <$> const[PV "warnings.pm"] s/BARE ->1e
# -        <;> ex-nextstate(B::Concise -924 Concise.pm:408) v:*,&,{,x*,x&,x$,$ ->1f
# -        <@> lineseq K ->-
# 1f          <;> nextstate(B::Concise -924 Concise.pm:408) :*,&,{,x*,x&,x$,$ ->1g
# 1k          <1> entersub[t1] KRS*/TARG,STRICT ->1l
# 1g             <0> pushmark s ->1h
# 1h             <$> const[PV "warnings"] sM ->1i
# 1i             <$> const[PV "qw"] sM ->1j
# 1j             <.> method_named[PV "unimport"] ->1k
# BEGIN 7:
# 1p <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->1p
# 1m       <;> nextstate(main 3 -e:1) v:>,<,%,{ ->1n
# 1o       <1> postinc[t3] sK/1 ->1p
# -           <1> ex-rv2sv sKRM/1 ->1o
# 1n             <#> gvsv[*beg] s ->1o
EOT_EOT
# BEGIN 1:
# 7  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->7
# 1        <;> nextstate(B::Concise -1151 Concise.pm:116) v:*,&,{,x*,x&,x$,$ ->2
# 6        <2> sassign sKS/2 ->7
# 4           <1> srefgen sK/1 ->5
# -              <1> ex-list lKRM ->4
# 3                 <1> rv2gv sKRM/STRICT,1 ->4
# 2                    <$> gv(*STDOUT) s ->3
# -           <1> ex-rv2sv sKRM*/STRICT,1 ->6
# 5              <$> gvsv(*B::Concise::walkHandle) s ->6
# BEGIN 2:
# h  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq K ->h
# 8        <;> nextstate(B::Concise -1113 Concise.pm:181) v:*,&,x*,x&,x$,$ ->9
# a        <1> require sK/1 ->b
# 9           <$> const(PV "strict.pm") s/BARE ->a
# -        <;> ex-nextstate(B::Concise -1113 Concise.pm:181) v:*,&,x*,x&,x$,$ ->b
# -        <@> lineseq K ->-
# b           <;> nextstate(B::Concise -1113 Concise.pm:181) :*,&,x*,x&,x$,$ ->c
# g           <1> entersub[t1] KRS*/TARG,STRICT ->h
# c              <0> pushmark s ->d
# d              <$> const(PV "strict") sM ->e
# e              <$> const(PV "refs") sM ->f
# f              <.> method_named(PV "unimport") ->g
# BEGIN 3:
# r  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq K ->r
# i        <;> nextstate(B::Concise -1010 Concise.pm:303) v:*,&,x*,x&,x$,$ ->j
# k        <1> require sK/1 ->l
# j           <$> const(PV "strict.pm") s/BARE ->k
# -        <;> ex-nextstate(B::Concise -1010 Concise.pm:303) v:*,&,x*,x&,x$,$ ->l
# -        <@> lineseq K ->-
# l           <;> nextstate(B::Concise -1010 Concise.pm:303) :*,&,x*,x&,x$,$ ->m
# q           <1> entersub[t1] KRS*/TARG,STRICT ->r
# m              <0> pushmark s ->n
# n              <$> const(PV "strict") sM ->o
# o              <$> const(PV "refs") sM ->p
# p              <.> method_named(PV "unimport") ->q
# BEGIN 4:
# 11 <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->11
# s        <;> nextstate(B::Concise -963 Concise.pm:368) v:*,&,{,x*,x&,x$,$ ->t
# u        <1> require sK/1 ->v
# t           <$> const(PV "strict.pm") s/BARE ->u
# -        <;> ex-nextstate(B::Concise -963 Concise.pm:368) v:*,&,{,x*,x&,x$,$ ->v
# -        <@> lineseq K ->-
# v           <;> nextstate(B::Concise -963 Concise.pm:368) :*,&,{,x*,x&,x$,$ ->w
# 10          <1> entersub[t1] KRS*/TARG,STRICT ->11
# w              <0> pushmark s ->x
# x              <$> const(PV "strict") sM ->y
# y              <$> const(PV "refs") sM ->z
# z              <.> method_named(PV "unimport") ->10
# BEGIN 5:
# 1b <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq K ->1b
# 12       <;> nextstate(B::Concise -938 Concise.pm:388) v:*,&,x*,x&,x$,$ ->13
# 14       <1> require sK/1 ->15
# 13          <$> const(PV "strict.pm") s/BARE ->14
# -        <;> ex-nextstate(B::Concise -938 Concise.pm:388) v:*,&,x*,x&,x$,$ ->15
# -        <@> lineseq K ->-
# 15          <;> nextstate(B::Concise -938 Concise.pm:388) :*,&,x*,x&,x$,$ ->16
# 1a          <1> entersub[t1] KRS*/TARG,STRICT ->1b
# 16             <0> pushmark s ->17
# 17             <$> const(PV "strict") sM ->18
# 18             <$> const(PV "refs") sM ->19
# 19             <.> method_named(PV "unimport") ->1a
# BEGIN 6:
# 1l <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->1l
# 1c       <;> nextstate(B::Concise -924 Concise.pm:408) v:*,&,{,x*,x&,x$,$ ->1d
# 1e       <1> require sK/1 ->1f
# 1d          <$> const(PV "warnings.pm") s/BARE ->1e
# -        <;> ex-nextstate(B::Concise -924 Concise.pm:408) v:*,&,{,x*,x&,x$,$ ->1f
# -        <@> lineseq K ->-
# 1f          <;> nextstate(B::Concise -924 Concise.pm:408) :*,&,{,x*,x&,x$,$ ->1g
# 1k          <1> entersub[t1] KRS*/TARG,STRICT ->1l
# 1g             <0> pushmark s ->1h
# 1h             <$> const(PV "warnings") sM ->1i
# 1i             <$> const(PV "qw") sM ->1j
# 1j             <.> method_named(PV "unimport") ->1k
# BEGIN 7:
# 1p <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->1p
# 1m       <;> nextstate(main 3 -e:1) v:>,<,%,{ ->1n
# 1o       <1> postinc[t2] sK/1 ->1p
# -           <1> ex-rv2sv sKRM/1 ->1o
# 1n             <$> gvsv(*beg) s ->1o
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
# 1  <;> nextstate(B::Concise -1151 Concise.pm:116) v:*,&,{,x*,x&,x$,$
# 2  <#> gv[*STDOUT] s
# 3  <1> rv2gv sKRM/STRICT,1
# 4  <1> srefgen sK/1
# 5  <#> gvsv[*B::Concise::walkHandle] s
# 6  <2> sassign sKS/2
# 7  <1> leavesub[1 ref] K/REFC,1
# BEGIN 2:
# 8  <;> nextstate(B::Concise -1113 Concise.pm:181) v:*,&,x*,x&,x$,$
# 9  <$> const[PV "strict.pm"] s/BARE
# a  <1> require sK/1
# b  <;> nextstate(B::Concise -1113 Concise.pm:181) :*,&,x*,x&,x$,$
# c  <0> pushmark s
# d  <$> const[PV "strict"] sM
# e  <$> const[PV "refs"] sM
# f  <.> method_named[PV "unimport"] 
# g  <1> entersub[t1] KRS*/TARG,STRICT
# h  <1> leavesub[1 ref] K/REFC,1
# BEGIN 3:
# i  <;> nextstate(B::Concise -1010 Concise.pm:303) v:*,&,x*,x&,x$,$
# j  <$> const[PV "strict.pm"] s/BARE
# k  <1> require sK/1
# l  <;> nextstate(B::Concise -1010 Concise.pm:303) :*,&,x*,x&,x$,$
# m  <0> pushmark s
# n  <$> const[PV "strict"] sM
# o  <$> const[PV "refs"] sM
# p  <.> method_named[PV "unimport"] 
# q  <1> entersub[t1] KRS*/TARG,STRICT
# r  <1> leavesub[1 ref] K/REFC,1
# BEGIN 4:
# s  <;> nextstate(B::Concise -963 Concise.pm:368) v:*,&,{,x*,x&,x$,$
# t  <$> const[PV "strict.pm"] s/BARE
# u  <1> require sK/1
# v  <;> nextstate(B::Concise -963 Concise.pm:368) :*,&,{,x*,x&,x$,$
# w  <0> pushmark s
# x  <$> const[PV "strict"] sM
# y  <$> const[PV "refs"] sM
# z  <.> method_named[PV "unimport"] 
# 10 <1> entersub[t1] KRS*/TARG,STRICT
# 11 <1> leavesub[1 ref] K/REFC,1
# BEGIN 5:
# 12 <;> nextstate(B::Concise -938 Concise.pm:388) v:*,&,x*,x&,x$,$
# 13 <$> const[PV "strict.pm"] s/BARE
# 14 <1> require sK/1
# 15 <;> nextstate(B::Concise -938 Concise.pm:388) :*,&,x*,x&,x$,$
# 16 <0> pushmark s
# 17 <$> const[PV "strict"] sM
# 18 <$> const[PV "refs"] sM
# 19 <.> method_named[PV "unimport"] 
# 1a <1> entersub[t1] KRS*/TARG,STRICT
# 1b <1> leavesub[1 ref] K/REFC,1
# BEGIN 6:
# 1c <;> nextstate(B::Concise -924 Concise.pm:408) v:*,&,{,x*,x&,x$,$
# 1d <$> const[PV "warnings.pm"] s/BARE
# 1e <1> require sK/1
# 1f <;> nextstate(B::Concise -924 Concise.pm:408) :*,&,{,x*,x&,x$,$
# 1g <0> pushmark s
# 1h <$> const[PV "warnings"] sM
# 1i <$> const[PV "qw"] sM
# 1j <.> method_named[PV "unimport"] 
# 1k <1> entersub[t1] KRS*/TARG,STRICT
# 1l <1> leavesub[1 ref] K/REFC,1
# BEGIN 7:
# 1m <;> nextstate(main 3 -e:1) v:>,<,%,{
# 1n <#> gvsv[*beg] s
# 1o <1> postinc[t3] sK/1
# 1p <1> leavesub[1 ref] K/REFC,1
# END 1:
# 1q <;> nextstate(main 9 -e:1) v:>,<,%,{
# 1r <#> gvsv[*end] s
# 1s <1> postinc[t3] sK/1
# 1t <1> leavesub[1 ref] K/REFC,1
# INIT 1:
# 1u <;> nextstate(main 7 -e:1) v:>,<,%,{
# 1v <#> gvsv[*init] s
# 1w <1> postinc[t3] sK/1
# 1x <1> leavesub[1 ref] K/REFC,1
# CHECK 1:
# 1y <;> nextstate(main 5 -e:1) v:>,<,%,{
# 1z <#> gvsv[*chk] s
# 20 <1> postinc[t3] sK/1
# 21 <1> leavesub[1 ref] K/REFC,1
# UNITCHECK 1:
# 22 <;> nextstate(main 11 -e:1) v:>,<,%,{
# 23 <#> gvsv[*uc] s
# 24 <1> postinc[t3] sK/1
# 25 <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# BEGIN 1:
# 1  <;> nextstate(B::Concise -1151 Concise.pm:116) v:*,&,{,x*,x&,x$,$
# 2  <$> gv(*STDOUT) s
# 3  <1> rv2gv sKRM/STRICT,1
# 4  <1> srefgen sK/1
# 5  <$> gvsv(*B::Concise::walkHandle) s
# 6  <2> sassign sKS/2
# 7  <1> leavesub[1 ref] K/REFC,1
# BEGIN 2:
# 8  <;> nextstate(B::Concise -1113 Concise.pm:181) v:*,&,x*,x&,x$,$
# 9  <$> const(PV "strict.pm") s/BARE
# a  <1> require sK/1
# b  <;> nextstate(B::Concise -1113 Concise.pm:181) :*,&,x*,x&,x$,$
# c  <0> pushmark s
# d  <$> const(PV "strict") sM
# e  <$> const(PV "refs") sM
# f  <.> method_named(PV "unimport") 
# g  <1> entersub[t1] KRS*/TARG,STRICT
# h  <1> leavesub[1 ref] K/REFC,1
# BEGIN 3:
# i  <;> nextstate(B::Concise -1010 Concise.pm:303) v:*,&,x*,x&,x$,$
# j  <$> const(PV "strict.pm") s/BARE
# k  <1> require sK/1
# l  <;> nextstate(B::Concise -1010 Concise.pm:303) :*,&,x*,x&,x$,$
# m  <0> pushmark s
# n  <$> const(PV "strict") sM
# o  <$> const(PV "refs") sM
# p  <.> method_named(PV "unimport") 
# q  <1> entersub[t1] KRS*/TARG,STRICT
# r  <1> leavesub[1 ref] K/REFC,1
# BEGIN 4:
# s  <;> nextstate(B::Concise -963 Concise.pm:368) v:*,&,{,x*,x&,x$,$
# t  <$> const(PV "strict.pm") s/BARE
# u  <1> require sK/1
# v  <;> nextstate(B::Concise -963 Concise.pm:368) :*,&,{,x*,x&,x$,$
# w  <0> pushmark s
# x  <$> const(PV "strict") sM
# y  <$> const(PV "refs") sM
# z  <.> method_named(PV "unimport") 
# 10 <1> entersub[t1] KRS*/TARG,STRICT
# 11 <1> leavesub[1 ref] K/REFC,1
# BEGIN 5:
# 12 <;> nextstate(B::Concise -938 Concise.pm:388) v:*,&,x*,x&,x$,$
# 13 <$> const(PV "strict.pm") s/BARE
# 14 <1> require sK/1
# 15 <;> nextstate(B::Concise -938 Concise.pm:388) :*,&,x*,x&,x$,$
# 16 <0> pushmark s
# 17 <$> const(PV "strict") sM
# 18 <$> const(PV "refs") sM
# 19 <.> method_named(PV "unimport") 
# 1a <1> entersub[t1] KRS*/TARG,STRICT
# 1b <1> leavesub[1 ref] K/REFC,1
# BEGIN 6:
# 1c <;> nextstate(B::Concise -924 Concise.pm:408) v:*,&,{,x*,x&,x$,$
# 1d <$> const(PV "warnings.pm") s/BARE
# 1e <1> require sK/1
# 1f <;> nextstate(B::Concise -924 Concise.pm:408) :*,&,{,x*,x&,x$,$
# 1g <0> pushmark s
# 1h <$> const(PV "warnings") sM
# 1i <$> const(PV "qw") sM
# 1j <.> method_named(PV "unimport") 
# 1k <1> entersub[t1] KRS*/TARG,STRICT
# 1l <1> leavesub[1 ref] K/REFC,1
# BEGIN 7:
# 1m <;> nextstate(main 3 -e:1) v:>,<,%,{
# 1n <$> gvsv(*beg) s
# 1o <1> postinc[t2] sK/1
# 1p <1> leavesub[1 ref] K/REFC,1
# END 1:
# 1q <;> nextstate(main 9 -e:1) v:>,<,%,{
# 1r <$> gvsv(*end) s
# 1s <1> postinc[t2] sK/1
# 1t <1> leavesub[1 ref] K/REFC,1
# INIT 1:
# 1u <;> nextstate(main 7 -e:1) v:>,<,%,{
# 1v <$> gvsv(*init) s
# 1w <1> postinc[t2] sK/1
# 1x <1> leavesub[1 ref] K/REFC,1
# CHECK 1:
# 1y <;> nextstate(main 5 -e:1) v:>,<,%,{
# 1z <$> gvsv(*chk) s
# 20 <1> postinc[t2] sK/1
# 21 <1> leavesub[1 ref] K/REFC,1
# UNITCHECK 1:
# 22 <;> nextstate(main 11 -e:1) v:>,<,%,{
# 23 <$> gvsv(*uc) s
# 24 <1> postinc[t2] sK/1
# 25 <1> leavesub[1 ref] K/REFC,1
EONT_EONT

# perl "-I../lib" -MO=Concise,BEGIN,CHECK,INIT,END,-exec -e '$a=$b && print q/foo/'

checkOptree ( name	=> 'regression test for patch 25352',
	      bcopts	=> [qw/ BEGIN END INIT CHECK -exec /],
	      prog	=> 'print q/foo/',
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# BEGIN 1:
# 1  <;> nextstate(B::Concise -275 Concise.pm:356) v:*,&,{,x*,x&,x$,$
# 2  <#> gv[*STDOUT] s
# 3  <1> rv2gv sKRM/STRICT,1
# 4  <1> srefgen sK/1
# 5  <#> gvsv[*B::Concise::walkHandle] s
# 6  <2> sassign sKS/2
# 7  <1> leavesub[1 ref] K/REFC,1
# BEGIN 2:
# 8  <;> nextstate(B::Concise -1113 Concise.pm:181) v:*,&,x*,x&,x$,$
# 9  <$> const[PV "strict.pm"] s/BARE
# a  <1> require sK/1
# b  <;> nextstate(B::Concise -1113 Concise.pm:181) :*,&,x*,x&,x$,$
# c  <0> pushmark s
# d  <$> const[PV "strict"] sM
# e  <$> const[PV "refs"] sM
# f  <.> method_named[PV "unimport"] 
# g  <1> entersub[t1] KRS*/TARG,STRICT
# h  <1> leavesub[1 ref] K/REFC,1
# BEGIN 3:
# i  <;> nextstate(B::Concise -1010 Concise.pm:303) v:*,&,x*,x&,x$,$
# j  <$> const[PV "strict.pm"] s/BARE
# k  <1> require sK/1
# l  <;> nextstate(B::Concise -1010 Concise.pm:303) :*,&,x*,x&,x$,$
# m  <0> pushmark s
# n  <$> const[PV "strict"] sM
# o  <$> const[PV "refs"] sM
# p  <.> method_named[PV "unimport"] 
# q  <1> entersub[t1] KRS*/TARG,STRICT
# r  <1> leavesub[1 ref] K/REFC,1
# BEGIN 4:
# s  <;> nextstate(B::Concise -963 Concise.pm:368) v:*,&,{,x*,x&,x$,$
# t  <$> const[PV "strict.pm"] s/BARE
# u  <1> require sK/1
# v  <;> nextstate(B::Concise -963 Concise.pm:368) :*,&,{,x*,x&,x$,$
# w  <0> pushmark s
# x  <$> const[PV "strict"] sM
# y  <$> const[PV "refs"] sM
# z  <.> method_named[PV "unimport"] 
# 10 <1> entersub[t1] KRS*/TARG,STRICT
# 11 <1> leavesub[1 ref] K/REFC,1
# BEGIN 5:
# 12 <;> nextstate(B::Concise -938 Concise.pm:388) v:*,&,x*,x&,x$,$
# 13 <$> const[PV "strict.pm"] s/BARE
# 14 <1> require sK/1
# 15 <;> nextstate(B::Concise -938 Concise.pm:388) :*,&,x*,x&,x$,$
# 16 <0> pushmark s
# 17 <$> const[PV "strict"] sM
# 18 <$> const[PV "refs"] sM
# 19 <.> method_named[PV "unimport"] 
# 1a <1> entersub[t1] KRS*/TARG,STRICT
# 1b <1> leavesub[1 ref] K/REFC,1
# BEGIN 6:
# 1c <;> nextstate(B::Concise -924 Concise.pm:408) v:*,&,{,x*,x&,x$,$
# 1d <$> const[PV "warnings.pm"] s/BARE
# 1e <1> require sK/1
# 1f <;> nextstate(B::Concise -924 Concise.pm:408) :*,&,{,x*,x&,x$,$
# 1g <0> pushmark s
# 1h <$> const[PV "warnings"] sM
# 1i <$> const[PV "qw"] sM
# 1j <.> method_named[PV "unimport"] 
# 1k <1> entersub[t1] KRS*/TARG,STRICT
# 1l <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# BEGIN 1:
# 1  <;> nextstate(B::Concise -1151 Concise.pm:116) v:*,&,{,x*,x&,x$,$
# 2  <$> gv(*STDOUT) s
# 3  <1> rv2gv sKRM/STRICT,1
# 4  <1> srefgen sK/1
# 5  <$> gvsv(*B::Concise::walkHandle) s
# 6  <2> sassign sKS/2
# 7  <1> leavesub[1 ref] K/REFC,1
# BEGIN 2:
# 8  <;> nextstate(B::Concise -1113 Concise.pm:181) v:*,&,x*,x&,x$,$
# 9  <$> const(PV "strict.pm") s/BARE
# a  <1> require sK/1
# b  <;> nextstate(B::Concise -1113 Concise.pm:181) :*,&,x*,x&,x$,$
# c  <0> pushmark s
# d  <$> const(PV "strict") sM
# e  <$> const(PV "refs") sM
# f  <.> method_named(PV "unimport") 
# g  <1> entersub[t1] KRS*/TARG,STRICT
# h  <1> leavesub[1 ref] K/REFC,1
# BEGIN 3:
# i  <;> nextstate(B::Concise -1010 Concise.pm:303) v:*,&,x*,x&,x$,$
# j  <$> const(PV "strict.pm") s/BARE
# k  <1> require sK/1
# l  <;> nextstate(B::Concise -1010 Concise.pm:303) :*,&,x*,x&,x$,$
# m  <0> pushmark s
# n  <$> const(PV "strict") sM
# o  <$> const(PV "refs") sM
# p  <.> method_named(PV "unimport") 
# q  <1> entersub[t1] KRS*/TARG,STRICT
# r  <1> leavesub[1 ref] K/REFC,1
# BEGIN 4:
# s  <;> nextstate(B::Concise -963 Concise.pm:368) v:*,&,{,x*,x&,x$,$
# t  <$> const(PV "strict.pm") s/BARE
# u  <1> require sK/1
# v  <;> nextstate(B::Concise -963 Concise.pm:368) :*,&,{,x*,x&,x$,$
# w  <0> pushmark s
# x  <$> const(PV "strict") sM
# y  <$> const(PV "refs") sM
# z  <.> method_named(PV "unimport") 
# 10 <1> entersub[t1] KRS*/TARG,STRICT
# 11 <1> leavesub[1 ref] K/REFC,1
# BEGIN 5:
# 12 <;> nextstate(B::Concise -938 Concise.pm:388) v:*,&,x*,x&,x$,$
# 13 <$> const(PV "strict.pm") s/BARE
# 14 <1> require sK/1
# 15 <;> nextstate(B::Concise -938 Concise.pm:388) :*,&,x*,x&,x$,$
# 16 <0> pushmark s
# 17 <$> const(PV "strict") sM
# 18 <$> const(PV "refs") sM
# 19 <.> method_named(PV "unimport") 
# 1a <1> entersub[t1] KRS*/TARG,STRICT
# 1b <1> leavesub[1 ref] K/REFC,1
# BEGIN 6:
# 1c <;> nextstate(B::Concise -924 Concise.pm:408) v:*,&,{,x*,x&,x$,$
# 1d <$> const(PV "warnings.pm") s/BARE
# 1e <1> require sK/1
# 1f <;> nextstate(B::Concise -924 Concise.pm:408) :*,&,{,x*,x&,x$,$
# 1g <0> pushmark s
# 1h <$> const(PV "warnings") sM
# 1i <$> const(PV "qw") sM
# 1j <.> method_named(PV "unimport") 
# 1k <1> entersub[t1] KRS*/TARG,STRICT
# 1l <1> leavesub[1 ref] K/REFC,1
EONT_EONT
