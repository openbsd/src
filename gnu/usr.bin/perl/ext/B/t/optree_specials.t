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

plan tests => 7;

require_ok("B::Concise");

my $out = runperl(
    switches => ["-MO=Concise,BEGIN,CHECK,INIT,END,-exec"],
    prog => q{$a=$b && print q/foo/},
    stderr => 1 );

#print "out:$out\n";

my $src = q[our ($beg, $chk, $init, $end) = qq{'foo'}; BEGIN { $beg++ } CHECK { $chk++ } INIT { $init++ } END { $end++ }];


my @warnings_todo;
@warnings_todo = (todo =>
   "Change 23768 (Remove Carp from warnings.pm) alters expected output, not"
   . "propagated to 5.8.x")
    if $] < 5.009;


checkOptree ( name	=> 'BEGIN',
	      bcopts	=> 'BEGIN',
	      prog	=> $src,
	      @warnings_todo,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# BEGIN 1:
# b  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->b
# 1        <;> nextstate(B::Concise -234 Concise.pm:328) v/2 ->2
# 3        <1> require sK/1 ->4
# 2           <$> const[PV "warnings.pm"] s/BARE ->3
# 4        <;> nextstate(B::Concise -234 Concise.pm:328) v/2 ->5
# -        <@> lineseq K ->-
# 5           <;> nextstate(B::Concise -234 Concise.pm:328) /2 ->6
# a           <1> entersub[t1] KS*/TARG,2 ->b
# 6              <0> pushmark s ->7
# 7              <$> const[PV "warnings"] sM ->8
# 8              <$> const[PV "qw"] sM ->9
# 9              <$> method_named[PVIV 1520340202] ->a
# BEGIN 2:
# f  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->f
# c        <;> nextstate(main 2 -e:1) v ->d
# e        <1> postinc[t3] sK/1 ->f
# -           <1> ex-rv2sv sKRM/1 ->e
# d              <#> gvsv[*beg] s ->e
EOT_EOT
# BEGIN 1:
# b  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->b
# 1        <;> nextstate(B::Concise -234 Concise.pm:328) v/2 ->2
# 3        <1> require sK/1 ->4
# 2           <$> const(PV "warnings.pm") s/BARE ->3
# 4        <;> nextstate(B::Concise -234 Concise.pm:328) v/2 ->5
# -        <@> lineseq K ->-
# 5           <;> nextstate(B::Concise -234 Concise.pm:328) /2 ->6
# a           <1> entersub[t1] KS*/TARG,2 ->b
# 6              <0> pushmark s ->7
# 7              <$> const(PV "warnings") sM ->8
# 8              <$> const(PV "qw") sM ->9
# 9              <$> method_named(PVIV 1520340202) ->a
# BEGIN 2:
# f  <1> leavesub[1 ref] K/REFC,1 ->(end)
# -     <@> lineseq KP ->f
# c        <;> nextstate(main 2 -e:1) v ->d
# e        <1> postinc[t2] sK/1 ->f
# -           <1> ex-rv2sv sKRM/1 ->e
# d              <$> gvsv(*beg) s ->e
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
	      prog	=> $src,
	      @warnings_todo,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# BEGIN 1:
# 1  <;> nextstate(B::Concise -234 Concise.pm:328) v/2
# 2  <$> const[PV "warnings.pm"] s/BARE
# 3  <1> require sK/1
# 4  <;> nextstate(B::Concise -234 Concise.pm:328) v/2
# 5  <;> nextstate(B::Concise -234 Concise.pm:328) /2
# 6  <0> pushmark s
# 7  <$> const[PV "warnings"] sM
# 8  <$> const[PV "qw"] sM
# 9  <$> method_named[PVIV 1520340202] 
# a  <1> entersub[t1] KS*/TARG,2
# b  <1> leavesub[1 ref] K/REFC,1
# BEGIN 2:
# c  <;> nextstate(main 2 -e:1) v
# d  <#> gvsv[*beg] s
# e  <1> postinc[t3] sK/1
# f  <1> leavesub[1 ref] K/REFC,1
# END 1:
# g  <;> nextstate(main 5 -e:1) v
# h  <#> gvsv[*end] s
# i  <1> postinc[t3] sK/1
# j  <1> leavesub[1 ref] K/REFC,1
# INIT 1:
# k  <;> nextstate(main 4 -e:1) v
# l  <#> gvsv[*init] s
# m  <1> postinc[t3] sK/1
# n  <1> leavesub[1 ref] K/REFC,1
# CHECK 1:
# o  <;> nextstate(main 3 -e:1) v
# p  <#> gvsv[*chk] s
# q  <1> postinc[t3] sK/1
# r  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# BEGIN 1:
# 1  <;> nextstate(B::Concise -234 Concise.pm:328) v/2
# 2  <$> const(PV "warnings.pm") s/BARE
# 3  <1> require sK/1
# 4  <;> nextstate(B::Concise -234 Concise.pm:328) v/2
# 5  <;> nextstate(B::Concise -234 Concise.pm:328) /2
# 6  <0> pushmark s
# 7  <$> const(PV "warnings") sM
# 8  <$> const(PV "qw") sM
# 9  <$> method_named(PVIV 1520340202) 
# a  <1> entersub[t1] KS*/TARG,2
# b  <1> leavesub[1 ref] K/REFC,1
# BEGIN 2:
# c  <;> nextstate(main 2 -e:1) v
# d  <$> gvsv(*beg) s
# e  <1> postinc[t2] sK/1
# f  <1> leavesub[1 ref] K/REFC,1
# END 1:
# g  <;> nextstate(main 5 -e:1) v
# h  <$> gvsv(*end) s
# i  <1> postinc[t2] sK/1
# j  <1> leavesub[1 ref] K/REFC,1
# INIT 1:
# k  <;> nextstate(main 4 -e:1) v
# l  <$> gvsv(*init) s
# m  <1> postinc[t2] sK/1
# n  <1> leavesub[1 ref] K/REFC,1
# CHECK 1:
# o  <;> nextstate(main 3 -e:1) v
# p  <$> gvsv(*chk) s
# q  <1> postinc[t2] sK/1
# r  <1> leavesub[1 ref] K/REFC,1
EONT_EONT


# perl "-I../lib" -MO=Concise,BEGIN,CHECK,INIT,END,-exec -e '$a=$b && print q/foo/'



checkOptree ( name	=> 'regression test for patch 25352',
	      bcopts	=> [qw/ BEGIN END INIT CHECK -exec /],
	      prog	=> 'print q/foo/',
	      @warnings_todo,
	      expect	=> <<'EOT_EOT', expect_nt => <<'EONT_EONT');
# BEGIN 1:
# 1  <;> nextstate(B::Concise -234 Concise.pm:359) v/2
# 2  <$> const[PV "warnings.pm"] s/BARE
# 3  <1> require sK/1
# 4  <;> nextstate(B::Concise -234 Concise.pm:359) v/2
# 5  <;> nextstate(B::Concise -234 Concise.pm:359) /2
# 6  <0> pushmark s
# 7  <$> const[PV "warnings"] sM
# 8  <$> const[PV "qw"] sM
# 9  <$> method_named[PV "unimport"] 
# a  <1> entersub[t1] KS*/TARG,2
# b  <1> leavesub[1 ref] K/REFC,1
EOT_EOT
# BEGIN 1:
# 1  <;> nextstate(B::Concise -234 Concise.pm:359) v/2
# 2  <$> const(PV "warnings.pm") s/BARE
# 3  <1> require sK/1
# 4  <;> nextstate(B::Concise -234 Concise.pm:359) v/2
# 5  <;> nextstate(B::Concise -234 Concise.pm:359) /2
# 6  <0> pushmark s
# 7  <$> const(PV "warnings") sM
# 8  <$> const(PV "qw") sM
# 9  <$> method_named(PV "unimport") 
# a  <1> entersub[t1] KS*/TARG,2
# b  <1> leavesub[1 ref] K/REFC,1
EONT_EONT
