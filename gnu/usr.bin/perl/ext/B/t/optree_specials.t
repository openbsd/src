#!./perl

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

# import checkOptree(), and %gOpts (containing test state)
use OptreeCheck;	# ALSO DOES @ARGV HANDLING !!!!!!
use Config;

plan tests => 6;

require_ok("B::Concise");

my $out = runperl(
    switches => ["-MO=Concise,BEGIN,CHECK,INIT,END,-exec"],
    prog => q{$a=$b && print q/foo/},
    stderr => 1 );

#print "out:$out\n";

my $src = q[our ($beg, $chk, $init, $end) = qq{'foo'}; BEGIN { $beg++ } CHECK { $chk++ } INIT { $init++ } END { $end++ }];



checkOptree ( name	=> 'BEGIN',
	      bcopts	=> 'BEGIN',
	      prog	=> $src,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# BEGIN 1:
# b  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->b
# 1        <;> nextstate(B::Concise -242 Concise.pm:304) v/2 ->2
# 3        <1> require sK/1 ->4
# 2           <$> const[PV "strict.pm"] s/BARE ->3
# 4        <;> nextstate(B::Concise -242 Concise.pm:304) v/2 ->5
# -        <@> lineseq K ->-
# 5           <;> nextstate(B::Concise -242 Concise.pm:304) /2 ->6
# a           <1> entersub[t1] KS*/TARG,2 ->b
# 6              <0> pushmark s ->7
# 7              <$> const[PV "strict"] sM ->8
# 8              <$> const[PV "refs"] sM ->9
# 9              <$> method_named[PVIV 1520340202] ->a
# BEGIN 2:
# m  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->m
# c        <;> nextstate(B::Concise -227 Concise.pm:327) v/2 ->d
# e        <1> require sK/1 ->f
# d           <$> const[PV "warnings.pm"] s/BARE ->e
# f        <;> nextstate(B::Concise -227 Concise.pm:327) v/2 ->g
# -        <@> lineseq K ->-
# g           <;> nextstate(B::Concise -227 Concise.pm:327) /2 ->h
# l           <1> entersub[t1] KS*/TARG,2 ->m
# h              <0> pushmark s ->i
# i              <$> const[PV "warnings"] sM ->j
# j              <$> const[PV "qw"] sM ->k
# k              <$> method_named[PVIV 1520340202] ->l
# BEGIN 3:
# q  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->q
# n        <;> nextstate(main 2 -e:3) v ->o
# p        <1> postinc[t3] sK/1 ->q
# -           <1> ex-rv2sv sKRM/1 ->p
# o              <#> gvsv[*beg] s ->p
EOT_EOT
# BEGIN 1:
# b  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->b
# 1        <;> nextstate(B::Concise -242 Concise.pm:304) v/2 ->2
# 3        <1> require sK/1 ->4
# 2           <$> const(PV "strict.pm") s/BARE ->3
# 4        <;> nextstate(B::Concise -242 Concise.pm:304) v/2 ->5
# -        <@> lineseq K ->-
# 5           <;> nextstate(B::Concise -242 Concise.pm:304) /2 ->6
# a           <1> entersub[t1] KS*/TARG,2 ->b
# 6              <0> pushmark s ->7
# 7              <$> const(PV "strict") sM ->8
# 8              <$> const(PV "refs") sM ->9
# 9              <$> method_named(PVIV 1520340202) ->a
# BEGIN 2:
# m  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->m
# c        <;> nextstate(B::Concise -227 Concise.pm:327) v/2 ->d
# e        <1> require sK/1 ->f
# d           <$> const(PV "warnings.pm") s/BARE ->e
# f        <;> nextstate(B::Concise -227 Concise.pm:327) v/2 ->g
# -        <@> lineseq K ->-
# g           <;> nextstate(B::Concise -227 Concise.pm:327) /2 ->h
# l           <1> entersub[t1] KS*/TARG,2 ->m
# h              <0> pushmark s ->i
# i              <$> const(PV "warnings") sM ->j
# j              <$> const(PV "qw") sM ->k
# k              <$> method_named(PVIV 1520340202) ->l
# BEGIN 3:
# q  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->q
# n        <;> nextstate(main 2 -e:3) v ->o
# p        <1> postinc[t2] sK/1 ->q
# -           <1> ex-rv2sv sKRM/1 ->p
# o              <$> gvsv(*beg) s ->p
EONT_EONT


checkOptree ( name	=> 'END',
	      bcopts	=> 'END',
	      prog	=> $src,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# END 1:
# 4  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->4
# 1        <;> nextstate(main 5 -e:6) v ->2
# 3        <1> postinc[t3] sK/1 ->4
# -           <1> ex-rv2sv sKRM/1 ->3
# 2              <#> gvsv[*end] s ->3
EOT_EOT
# END 1:
# 4  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->4
# 1        <;> nextstate(main 5 -e:6) v ->2
# 3        <1> postinc[t2] sK/1 ->4
# -           <1> ex-rv2sv sKRM/1 ->3
# 2              <$> gvsv(*end) s ->3
EONT_EONT


checkOptree ( name	=> 'CHECK',
	      bcopts	=> 'CHECK',
	      prog	=> $src,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# CHECK 1:
# 4  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->4
# 1        <;> nextstate(main 3 -e:4) v ->2
# 3        <1> postinc[t3] sK/1 ->4
# -           <1> ex-rv2sv sKRM/1 ->3
# 2              <#> gvsv[*chk] s ->3
EOT_EOT
# CHECK 1:
# 4  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->4
# 1        <;> nextstate(main 3 -e:4) v ->2
# 3        <1> postinc[t2] sK/1 ->4
# -           <1> ex-rv2sv sKRM/1 ->3
# 2              <$> gvsv(*chk) s ->3
EONT_EONT


checkOptree ( name	=> 'INIT',
	      bcopts	=> 'INIT',
	      #todo	=> 'get working',
	      prog	=> $src,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# INIT 1:
# 4  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->4
# 1        <;> nextstate(main 4 -e:5) v ->2
# 3        <1> postinc[t3] sK/1 ->4
# -           <1> ex-rv2sv sKRM/1 ->3
# 2              <#> gvsv[*init] s ->3
EOT_EOT
# INIT 1:
# 4  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->4
# 1        <;> nextstate(main 4 -e:5) v ->2
# 3        <1> postinc[t2] sK/1 ->4
# -           <1> ex-rv2sv sKRM/1 ->3
# 2              <$> gvsv(*init) s ->3
EONT_EONT


checkOptree ( name	=> 'all of BEGIN END INIT CHECK -exec',
	      bcopts	=> [qw/ BEGIN END INIT CHECK -exec /],
	      #todo	=> 'get working',
	      prog	=> $src,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# BEGIN 1:
# 1  <;> nextstate(B::Concise -242 Concise.pm:304) v/2
# 2  <$> const[PV "strict.pm"] s/BARE
# 3  <1> require sK/1
# 4  <;> nextstate(B::Concise -242 Concise.pm:304) v/2
# 5  <;> nextstate(B::Concise -242 Concise.pm:304) /2
# 6  <0> pushmark s
# 7  <$> const[PV "strict"] sM
# 8  <$> const[PV "refs"] sM
# 9  <$> method_named[PVIV 1520340202] 
# a  <1> entersub[t1] KS*/TARG,2
# b  <1> leavesub[1 ref] K/REFC,1
# BEGIN 2:
# c  <;> nextstate(B::Concise -227 Concise.pm:327) v/2
# d  <$> const[PV "warnings.pm"] s/BARE
# e  <1> require sK/1
# f  <;> nextstate(B::Concise -227 Concise.pm:327) v/2
# g  <;> nextstate(B::Concise -227 Concise.pm:327) /2
# h  <0> pushmark s
# i  <$> const[PV "warnings"] sM
# j  <$> const[PV "qw"] sM
# k  <$> method_named[PVIV 1520340202] 
# l  <1> entersub[t1] KS*/TARG,2
# m  <1> leavesub[1 ref] K/REFC,1
# BEGIN 3:
# n  <;> nextstate(main 2 -e:3) v
# o  <#> gvsv[*beg] s
# p  <1> postinc[t3] sK/1
# q  <1> leavesub[1 ref] K/REFC,1
# END 1:
# r  <;> nextstate(main 5 -e:6) v
# s  <#> gvsv[*end] s
# t  <1> postinc[t3] sK/1
# u  <1> leavesub[1 ref] K/REFC,1
# INIT 1:
# v  <;> nextstate(main 4 -e:5) v
# w  <#> gvsv[*init] s
# x  <1> postinc[t3] sK/1
# y  <1> leavesub[1 ref] K/REFC,1
# CHECK 1:
# z  <;> nextstate(main 3 -e:4) v
# 10 <#> gvsv[*chk] s
# 11 <1> postinc[t3] sK/1
# 12 <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# BEGIN 1:
# 1  <;> nextstate(B::Concise -242 Concise.pm:304) v/2
# 2  <$> const(PV "strict.pm") s/BARE
# 3  <1> require sK/1
# 4  <;> nextstate(B::Concise -242 Concise.pm:304) v/2
# 5  <;> nextstate(B::Concise -242 Concise.pm:304) /2
# 6  <0> pushmark s
# 7  <$> const(PV "strict") sM
# 8  <$> const(PV "refs") sM
# 9  <$> method_named(PVIV 1520340202) 
# a  <1> entersub[t1] KS*/TARG,2
# b  <1> leavesub[1 ref] K/REFC,1
# BEGIN 2:
# c  <;> nextstate(B::Concise -227 Concise.pm:327) v/2
# d  <$> const(PV "warnings.pm") s/BARE
# e  <1> require sK/1
# f  <;> nextstate(B::Concise -227 Concise.pm:327) v/2
# g  <;> nextstate(B::Concise -227 Concise.pm:327) /2
# h  <0> pushmark s
# i  <$> const(PV "warnings") sM
# j  <$> const(PV "qw") sM
# k  <$> method_named(PVIV 1520340202) 
# l  <1> entersub[t1] KS*/TARG,2
# m  <1> leavesub[1 ref] K/REFC,1
# BEGIN 3:
# n  <;> nextstate(main 2 -e:3) v
# o  <$> gvsv(*beg) s
# p  <1> postinc[t2] sK/1
# q  <1> leavesub[1 ref] K/REFC,1
# END 1:
# r  <;> nextstate(main 5 -e:6) v
# s  <$> gvsv(*end) s
# t  <1> postinc[t2] sK/1
# u  <1> leavesub[1 ref] K/REFC,1
# INIT 1:
# v  <;> nextstate(main 4 -e:5) v
# w  <$> gvsv(*init) s
# x  <1> postinc[t2] sK/1
# y  <1> leavesub[1 ref] K/REFC,1
# CHECK 1:
# z  <;> nextstate(main 3 -e:4) v
# 10 <$> gvsv(*chk) s
# 11 <1> postinc[t2] sK/1
# 12 <1> leavesub[1 ref] K/REFC,1
EONT_EONT
