#!./perl

# A place to put some simple leak tests. Uses XS::APItest to make
# PL_sv_count available, allowing us to run a bit of code multiple times and
# see if the count increases.

BEGIN {
    chdir 't';
    @INC = '../lib';
    require './test.pl';

    eval { require XS::APItest; XS::APItest->import('sv_count'); 1 }
	or skip_all("XS::APItest not available");
}

plan tests => 21;

# run some code N times. If the number of SVs at the end of loop N is
# greater than (N-1)*delta at the end of loop 1, we've got a leak
#
sub leak {
    my ($n, $delta, $code, @rest) = @_;
    my $sv0 = 0;
    my $sv1 = 0;
    for my $i (1..$n) {
	&$code();
	$sv1 = sv_count();
	$sv0 = $sv1 if $i == 1;
    }
    cmp_ok($sv1-$sv0, '<=', ($n-1)*$delta, @rest);
}

# run some expression N times. The expr is concatenated N times and then
# evaled, ensuring that that there are no scope exits between executions.
# If the number of SVs at the end of expr N is greater than (N-1)*delta at
# the end of expr 1, we've got a leak
#
sub leak_expr {
    my ($n, $delta, $expr, @rest) = @_;
    my $sv0 = 0;
    my $sv1 = 0;
    my $true = 1; # avoid stuff being optimised away
    my $code1 = "($expr || \$true)";
    my $code = "$code1 && (\$sv0 = sv_count())" . ("&& $code1" x 4)
		. " && (\$sv1 = sv_count())";
    if (eval $code) {
	cmp_ok($sv1-$sv0, '<=', ($n-1)*$delta, @rest);
    }
    else {
	fail("eval @rest: $@");
    }
}


my @a;

leak(5, 0, sub {},                 "basic check 1 of leak test infrastructure");
leak(5, 0, sub {push @a,1;pop @a}, "basic check 2 of leak test infrastructure");
leak(5, 1, sub {push @a,1;},       "basic check 3 of leak test infrastructure");

sub TIEARRAY	{ bless [], $_[0] }
sub FETCH	{ $_[0]->[$_[1]] }
sub STORE	{ $_[0]->[$_[1]] = $_[2] }

# local $tied_elem[..] leaks <20020502143736.N16831@dansat.data-plan.com>"
{
    tie my @a, 'main';
    leak(5, 0, sub {local $a[0]}, "local \$tied[0]");
}

# [perl #74484]  repeated tries leaked SVs on the tmps stack

leak_expr(5, 0, q{"YYYYYa" =~ /.+?(a(.+?)|b)/ }, "trie leak");

# [perl #48004] map/grep didn't free tmps till the end

{
    # qr/1/ just creates tmps that are hopefully freed per iteration

    my $s;
    my @a;
    my @count = (0) x 4; # pre-allocate

    grep qr/1/ && ($count[$_] = sv_count()) && 99,  0..3;
    is(@count[3] - @count[0], 0, "void   grep expr:  no new tmps per iter");
    grep { qr/1/ && ($count[$_] = sv_count()) && 99 }  0..3;
    is(@count[3] - @count[0], 0, "void   grep block: no new tmps per iter");

    $s = grep qr/1/ && ($count[$_] = sv_count()) && 99,  0..3;
    is(@count[3] - @count[0], 0, "scalar grep expr:  no new tmps per iter");
    $s = grep { qr/1/ && ($count[$_] = sv_count()) && 99 }  0..3;
    is(@count[3] - @count[0], 0, "scalar grep block: no new tmps per iter");

    @a = grep qr/1/ && ($count[$_] = sv_count()) && 99,  0..3;
    is(@count[3] - @count[0], 0, "list   grep expr:  no new tmps per iter");
    @a = grep { qr/1/ && ($count[$_] = sv_count()) && 99 }  0..3;
    is(@count[3] - @count[0], 0, "list   grep block: no new tmps per iter");


    map qr/1/ && ($count[$_] = sv_count()) && 99,  0..3;
    is(@count[3] - @count[0], 0, "void   map expr:  no new tmps per iter");
    map { qr/1/ && ($count[$_] = sv_count()) && 99 }  0..3;
    is(@count[3] - @count[0], 0, "void   map block: no new tmps per iter");

    $s = map qr/1/ && ($count[$_] = sv_count()) && 99,  0..3;
    is(@count[3] - @count[0], 0, "scalar map expr:  no new tmps per iter");
    $s = map { qr/1/ && ($count[$_] = sv_count()) && 99 }  0..3;
    is(@count[3] - @count[0], 0, "scalar map block: no new tmps per iter");

    @a = map qr/1/ && ($count[$_] = sv_count()) && 99,  0..3;
    is(@count[3] - @count[0], 3, "list   map expr:  one new tmp per iter");
    @a = map { qr/1/ && ($count[$_] = sv_count()) && 99 }  0..3;
    is(@count[3] - @count[0], 3, "list   map block: one new tmp per iter");

}

SKIP:
{ # broken by 304474c3, fixed by cefd5c7c, but didn't seem to cause
  # any other test failures
  # base test case from ribasushi (Peter Rabbitson)
  eval { require Scalar::Util; Scalar::Util->import("weaken"); 1; }
    or skip "no weaken", 1;
  my $weak;
  {
    $weak = my $in = {};
    weaken($weak);
    my $out = { in => $in, in => undef }
  }
  ok(!$weak, "hash referenced weakened SV released");
}

# RT #72246: rcatline memory leak on bad $/

leak(2, 0,
    sub {
	my $f;
	open CATLINE, '<', \$f;
	local $/ = "\x{262E}";
	my $str = "\x{2622}";
	eval { $str .= <CATLINE> };
    },
    "rcatline leak"
);

{
    my $RE = qr/
      (?:
        <(?<tag>
          \s*
          [^>\s]+
        )>
      )??
    /xis;

    "<html><body></body></html>" =~ m/$RE/gcs;

    leak(5, 0, sub {
        my $tag = $+{tag};
    }, "named regexp captures");
}

leak(2,0,sub { !$^V }, '[perl #109762] version object in boolean context');
