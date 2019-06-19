#!./perl

#
# Regression tests for the Math::Complex pacakge
# -- Raphael Manfredi	since Sep 1996
# -- Jarkko Hietaniemi	since Mar 1997
# -- Daniel S. Lewart	since Sep 1997

use Math::Complex 1.54;

our $vax_float = (pack("d",1) =~ /^[\x80\x10]\x40/);
our $has_inf   = !$vax_float;

my ($args, $op, $target, $test, $test_set, $try, $val, $zvalue, @set, @val);

$test = 0;
$| = 1;
my @script = (
    'my ($res, $s0,$s1,$s2,$s3,$s4,$s5,$s6,$s7,$s8,$s9,$s10,$z0,$z1,$z2);' .
	"\n\n"
);
my $eps = 1e-13;

if ($^O eq 'unicos') { 	# For some reason root() produces very inaccurate
    $eps = 1e-10;	# results in Cray UNICOS, and occasionally also
}			# cos(), sin(), cosh(), sinh().  The division
			# of doubles is the current suspect.

$test++;
push @script, "{ my \$t=$test; ".q{
    my $a = Math::Complex->new(1);
    my $b = $a;
    $a += 2;
    print "not " unless "$a" eq "3" && "$b" eq "1";
    print "ok $t\n";
}."}";

while (<DATA>) {
	s/^\s+//;
	next if $_ eq '' || /^\#/;
	chomp;
	$test_set = 0;		# Assume not a test over a set of values
	if (/^&(.+)/) {
		$op = $1;
		next;
	}
	elsif (/^\{(.+)\}/) {
		set($1, \@set, \@val);
		next;
	}
	elsif (s/^\|//) {
		$test_set = 1;	# Requests we loop over the set...
	}
	my @args = split(/:/);
	if ($test_set == 1) {
		my $i;
		for ($i = 0; $i < @set; $i++) {
			# complex number
			$target = $set[$i];
			# textual value as found in set definition
			$zvalue = $val[$i];
			test($zvalue, $target, @args);
		}
	} else {
		test($op, undef, @args);
	}
}

#

sub test_mutators {
    my $op;

    $test++;
push(@script, <<'EOT');
{
    my $z = cplx(  1,  1);
    $z->Re(2);
    $z->Im(3);
    print "# $test Re(z) = ",$z->Re(), " Im(z) = ", $z->Im(), " z = $z\n";
    print 'not ' unless Re($z) == 2 and Im($z) == 3;
EOT
    push(@script, qq(print "ok $test\\n"}\n));

    $test++;
push(@script, <<'EOT');
{
    my $z = cplx(  1,  1);
    $z->abs(3 * sqrt(2));
    print "# $test Re(z) = ",$z->Re(), " Im(z) = ", $z->Im(), " z = $z\n";
    print 'not ' unless (abs($z) - 3 * sqrt(2)) < $eps and
                        (arg($z) - pi / 4     ) < $eps and
                        (Re($z) - 3           ) < $eps and
                        (Im($z) - 3           ) < $eps;
EOT
    push(@script, qq(print "ok $test\\n"}\n));

    $test++;
push(@script, <<'EOT');
{
    my $z = cplx(  1,  1);
    $z->arg(-3 / 4 * pi);
    print "# $test Re(z) = ",$z->Re(), " Im(z) = ", $z->Im(), " z = $z\n";
    print 'not ' unless (arg($z) + 3 / 4 * pi) < $eps and
                        (abs($z) - sqrt(2)   ) < $eps and
                        (Re($z) + 1          ) < $eps and
                        (Im($z) + 1          ) < $eps;
EOT
    push(@script, qq(print "ok $test\\n"}\n));
}

test_mutators();

my $constants = '
my $i    = cplx(0,  1);
my $pi   = cplx(pi, 0);
my $pii  = cplx(0, pi);
my $pip2 = cplx(pi/2, 0);
my $pip4 = cplx(pi/4, 0);
my $zero = cplx(0, 0);
';

if ($has_inf) {
    $constants .= <<'EOF';
my $inf  = 9**9**9;
EOF
}

push(@script, $constants);


# test the divbyzeros

sub test_dbz {
    for my $op (@_) {
	$test++;
	push(@script, <<EOT);
	eval '$op';
	(\$bad) = (\$@ =~ /(.+)/);
	print "# $test op = $op divbyzero? \$bad...\n";
	print 'not ' unless (\$@ =~ /Division by zero/);
EOT
        push(@script, qq(print "ok $test\\n";\n));
    }
}

# test the logofzeros

sub test_loz {
    for my $op (@_) {
	$test++;
	push(@script, <<EOT);
	eval '$op';
	(\$bad) = (\$@ =~ /(.+)/);
	print "# $test op = $op logofzero? \$bad...\n";
	print 'not ' unless (\$@ =~ /Logarithm of zero/);
EOT
        push(@script, qq(print "ok $test\\n";\n));
    }
}

test_dbz(
	 'i/0',
	 'acot(0)',
	 'acot(+$i)',
#	 'acoth(-1)',	# Log of zero.
	 'acoth(0)',
	 'acoth(+1)',
	 'acsc(0)',
	 'acsch(0)',
	 'asec(0)',
	 'asech(0)',
	 'atan($i)',
#	 'atanh(-1)',	# Log of zero.
	 'atanh(+1)',
	 'cot(0)',
	 'coth(0)',
	 'csc(0)',
	 'csch(0)',
	 'atan(cplx(0, 1), cplx(1, 0))',
	);

test_loz(
	 'log($zero)',
	 'atan(-$i)',
	 'acot(-$i)',
	 'atanh(-1)',
	 'acoth(-1)',
	);

# test the bad roots

sub test_broot {
    for my $op (@_) {
	$test++;
	push(@script, <<EOT);
	eval 'root(2, $op)';
	(\$bad) = (\$@ =~ /(.+)/);
	print "# $test op = $op badroot? \$bad...\n";
	print 'not ' unless (\$@ =~ /root rank must be/);
EOT
        push(@script, qq(print "ok $test\\n";\n));
    }
}

test_broot(qw(-3 -2.1 0 0.99));

sub test_display_format {
    $test++;
    push @script, <<EOS;
    print "# package display_format cartesian?\n";
    print "not " unless Math::Complex->display_format eq 'cartesian';
    print "ok $test\n";
EOS

    push @script, <<EOS;
    my \$j = (root(1,3))[1];

    \$j->display_format('polar');
EOS

    $test++;
    push @script, <<EOS;
    print "# j display_format polar?\n";
    print "not " unless \$j->display_format eq 'polar';
    print "ok $test\n";
EOS

    $test++;
    push @script, <<EOS;
    print "# j = \$j\n";
    print "not " unless "\$j" eq "[1,2pi/3]";
    print "ok $test\n";

    my %display_format;

    %display_format = \$j->display_format;
EOS

    $test++;
    push @script, <<EOS;
    print "# display_format{style} polar?\n";
    print "not " unless \$display_format{style} eq 'polar';
    print "ok $test\n";
EOS

    $test++;
    push @script, <<EOS;
    print "# keys %display_format == 2?\n";
    print "not " unless keys %display_format == 2;
    print "ok $test\n";

    \$j->display_format('style' => 'cartesian', 'format' => '%.5f');
EOS

    $test++;
    push @script, <<EOS;
    print "# j = \$j\n";
    print "not " unless "\$j" eq "-0.50000+0.86603i";
    print "ok $test\n";

    %display_format = \$j->display_format;
EOS

    $test++;
    push @script, <<EOS;
    print "# display_format{format} %.5f?\n";
    print "not " unless \$display_format{format} eq '%.5f';
    print "ok $test\n";
EOS

    $test++;
    push @script, <<EOS;
    print "# keys %display_format == 3?\n";
    print "not " unless keys %display_format == 3;
    print "ok $test\n";

    \$j->display_format('format' => undef);
EOS

    $test++;
    push @script, <<EOS;
    print "# j = \$j\n";
    print "not " unless "\$j" =~ /^-0(?:\\.5(?:0000\\d+)?|\\.49999\\d+)\\+0.86602540\\d+i\$/;
    print "ok $test\n";

    \$j->display_format('style' => 'polar', 'polar_pretty_print' => 0);
EOS

    $test++;
    push @script, <<EOS;
    print "# j = \$j\n";
    print "not " unless "\$j" =~ /^\\[1,2\\.09439510\\d+\\]\$/;
    print "ok $test\n";

    \$j->display_format('style' => 'polar', 'format' => "%.4g");
EOS

    $test++;
    push @script, <<EOS;
    print "# j = \$j\n";
    print "not " unless "\$j" =~ /^\\[1,2\\.094\\]\$/;
    print "ok $test\n";

    \$j->display_format('style' => 'cartesian', 'format' => '(%.5g)');
EOS

    $test++;
    push @script, <<EOS;
    print "# j = \$j\n";
    print "not " unless "\$j" eq "(-0.5)+(0.86603)i";
    print "ok $test\n";
EOS

    $test++;
    push @script, <<EOS;
    print "# j display_format cartesian?\n";
    print "not " unless \$j->display_format eq 'cartesian';
    print "ok $test\n";
EOS
}

test_display_format();

sub test_remake {
    $test++;
    push @script, <<EOS;
    print "# remake 2+3i\n";
    \$z = cplx('2+3i');
    print "not " unless \$z == Math::Complex->make(2,3);
    print "ok $test\n";
EOS

    $test++;
    push @script, <<EOS;
    print "# make 3i\n";
    \$z = Math::Complex->make('3i');
    print "not " unless \$z == cplx(0,3);
    print "ok $test\n";
EOS

    $test++;
    push @script, <<EOS;
    print "# emake [2,3]\n";
    \$z = Math::Complex->emake('[2,3]');
    print "not " unless \$z == cplxe(2,3);
    print "ok $test\n";
EOS

    $test++;
    push @script, <<EOS;
    print "# make (2,3)\n";
    \$z = Math::Complex->make('(2,3)');
    print "not " unless \$z == cplx(2,3);
    print "ok $test\n";
EOS

    $test++;
    push @script, <<EOS;
    print "# emake [2,3pi/8]\n";
    \$z = Math::Complex->emake('[2,3pi/8]');
    print "not " unless \$z == cplxe(2,3*\$pi/8);
    print "ok $test\n";
EOS

    $test++;
    push @script, <<EOS;
    print "# emake [2]\n";
    \$z = Math::Complex->emake('[2]');
    print "not " unless \$z == cplxe(2);
    print "ok $test\n";
EOS
}

sub test_no_args {
    push @script, <<'EOS';
{
    print "# cplx, cplxe, make, emake without arguments\n";
EOS

    $test++;
    push @script, <<EOS;
    my \$z0 = cplx();
    print ((\$z0->Re()  == 0) ? "ok $test\n" : "not ok $test\n");
EOS

    $test++;
    push @script, <<EOS;
    print ((\$z0->Im()  == 0) ? "ok $test\n" : "not ok $test\n");
EOS

    $test++;
    push @script, <<EOS;
    my \$z1 = cplxe();
    print ((\$z1->rho()   == 0) ? "ok $test\n" : "not ok $test\n");
EOS

    $test++;
    push @script, <<EOS;
    print ((\$z1->theta() == 0) ? "ok $test\n" : "not ok $test\n");
EOS

    $test++;
    push @script, <<EOS;
    my \$z2 = Math::Complex->make();
    print ((\$z2->Re()  == 0) ? "ok $test\n" : "not ok $test\n");
EOS

    $test++;
    push @script, <<EOS;
    print ((\$z2->Im()  == 0) ? "ok $test\n" : "not ok $test\n");
EOS

    $test++;
    push @script, <<EOS;
    my \$z3 = Math::Complex->emake();
    print ((\$z3->rho()   == 0) ? "ok $test\n" : "not ok $test\n");
EOS

    $test++;
    push @script, <<EOS;
    print ((\$z3->theta() == 0) ? "ok $test\n" : "not ok $test\n");
}
EOS
}

sub test_atan2 {
    push @script, <<'EOS';
print "# atan2() with some real arguments\n";
EOS
    my @real = (-1, 0, 1);
    for my $x (@real) {
	for my $y (@real) {
	    next if $x == 0 && $y == 0;
	    $test++;
	    push @script, <<EOS;
print ((Math::Complex::atan2($y, $x) == CORE::atan2($y, $x)) ? "ok $test\n" : "not ok $test\n");
EOS
        }
    }
    push @script, <<'EOS';
    print "# atan2() with some complex arguments\n";
EOS
    $test++;
    push @script, <<EOS;
    print (abs(atan2(0, cplx(0, 1))) < $eps ? "ok $test\n" : "not ok $test\n");
EOS
    $test++;
    push @script, <<EOS;
    print (abs(atan2(cplx(0, 1), 0) - \$pip2) < $eps ? "ok $test\n" : "not ok $test\n");
EOS
    $test++;
    push @script, <<EOS;
    print (abs(atan2(cplx(0, 1), cplx(0, 1)) - \$pip4) < $eps ? "ok $test\n" : "not ok $test\n");
EOS
    $test++;
    push @script, <<EOS;
    print (abs(atan2(cplx(0, 1), cplx(1, 1)) - cplx(0.553574358897045, 0.402359478108525)) < $eps ? "ok $test\n" : "not ok $test\n");
EOS
}

sub test_decplx {
}

test_remake();

test_no_args();

test_atan2();

test_decplx();

print "1..$test\n";
#print @script, "\n";
eval join '', @script;
die $@ if $@;

sub abop {
	my ($op) = @_;

	push(@script, qq(print "# $op=\n";));
}

sub test {
	my ($op, $z, @args) = @_;
	my ($baop) = 0;
	$test++;
	my $i;
	$baop = 1 if ($op =~ s/;=$//);
	for ($i = 0; $i < @args; $i++) {
		$val = value($args[$i]);
		push @script, "\$z$i = $val;\n";
	}
	if (defined $z) {
		$args = "'$op'";		# Really the value
		$try = "abs(\$z0 - \$z1) <= $eps ? \$z1 : \$z0";
		push @script, "\$res = $try; ";
		push @script, "check($test, $args[0], \$res, \$z$#args, $args);\n";
	} else {
		my ($try, $args);
		if (@args == 2) {
			$try = "$op \$z0";
			$args = "'$args[0]'";
		} else {
			$try = ($op =~ /^\w/) ? "$op(\$z0, \$z1)" : "\$z0 $op \$z1";
			$args = "'$args[0]', '$args[1]'";
		}
		push @script, "\$res = $try; ";
		push @script, "check($test, '$try', \$res, \$z$#args, $args);\n";
		if (@args > 2 and $baop) { # binary assignment ops
			$test++;
			# check the op= works
			push @script, <<EOB;
{
	my \$za = cplx(ref \$z0 ? \@{\$z0->_cartesian} : (\$z0, 0));

	my (\$z1r, \$z1i) = ref \$z1 ? \@{\$z1->_cartesian} : (\$z1, 0);

	my \$zb = cplx(\$z1r, \$z1i);

	\$za $op= \$zb;
	my (\$zbr, \$zbi) = \@{\$zb->_cartesian};

	check($test, '\$z0 $op= \$z1', \$za, \$z$#args, $args);
EOB
			$test++;
			# check that the rhs has not changed
			push @script, qq(print "not " unless (\$zbr == \$z1r and \$zbi == \$z1i););
			push @script, qq(print "ok $test\\n";\n);
			push @script, "}\n";
		}
	}
}

sub set {
	my ($set, $setref, $valref) = @_;
	@{$setref} = ();
	@{$valref} = ();
	my @set = split(/;\s*/, $set);
	my @res;
	my $i;
	for ($i = 0; $i < @set; $i++) {
		push(@{$valref}, $set[$i]);
		my $val = value($set[$i]);
		push @script, "\$s$i = $val;\n";
		push @{$setref}, "\$s$i";
	}
}

sub value {
	local ($_) = @_;
	if (/^\s*\((.*),(.*)\)/) {
		return "cplx($1,$2)";
	}
	elsif (/^\s*([\-\+]?(?:\d+(\.\d+)?|\.\d+)(?:[e[\-\+]\d+])?)/) {
		return "cplx($1,0)";
	}
	elsif (/^\s*\[(.*),(.*)\]/) {
		return "cplxe($1,$2)";
	}
	elsif (/^\s*'(.*)'/) {
		my $ex = $1;
		$ex =~ s/\bz\b/$target/g;
		$ex =~ s/\br\b/abs($target)/g;
		$ex =~ s/\bt\b/arg($target)/g;
		$ex =~ s/\ba\b/Re($target)/g;
		$ex =~ s/\bb\b/Im($target)/g;
		return $ex;
	}
	elsif (/^\s*"(.*)"/) {
		return "\"$1\"";
	}
	return $_;
}

sub check {
	my ($test, $try, $got, $expected, @z) = @_;

	print "# @_\n";

	if ("$got" eq "$expected"
	    ||
	    ($expected =~ /^-?\d/ && $got == $expected)
	    ||
	    (abs(Math::Complex->make($got) - Math::Complex->make($expected)) < $eps)
	    ||
	    (abs($got - $expected) < $eps)
	    ) {
		print "ok $test\n";
	} else {
		print "not ok $test\n";
		my $args = (@z == 1) ? "z = $z[0]" : "z0 = $z[0], z1 = $z[1]";
		print "# '$try' expected: '$expected' got: '$got' for $args\n";
	}
}

sub addsq {
    my ($z1, $z2) = @_;
    return ($z1 + i*$z2) * ($z1 - i*$z2);
}

sub subsq {
    my ($z1, $z2) = @_;
    return ($z1 + $z2) * ($z1 - $z2);
}

__END__
&+;=
(3,4):(3,4):(6,8)
(-3,4):(3,-4):(0,0)
(3,4):-3:(0,4)
1:(4,2):(5,2)
[2,0]:[2,pi]:(0,0)

&++
(2,1):(3,1)

&-;=
(2,3):(-2,-3)
[2,pi/2]:[2,-(pi)/2]
2:[2,0]:(0,0)
[3,0]:2:(1,0)
3:(4,5):(-1,-5)
(4,5):3:(1,5)
(2,1):(3,5):(-1,-4)

&--
(1,2):(0,2)
[2,pi]:[3,pi]

&*;=
(0,1):(0,1):(-1,0)
(4,5):(1,0):(4,5)
[2,2*pi/3]:(1,0):[2,2*pi/3]
2:(0,1):(0,2)
(0,1):3:(0,3)
(0,1):(4,1):(-1,4)
(2,1):(4,-1):(9,2)

&/;=
(3,4):(3,4):(1,0)
(4,-5):1:(4,-5)
1:(0,1):(0,-1)
(0,6):(0,2):(3,0)
(9,2):(4,-1):(2,1)
[4,pi]:[2,pi/2]:[2,pi/2]
[2,pi/2]:[4,pi]:[0.5,-(pi)/2]

&**;=
(2,0):(3,0):(8,0)
(3,0):(2,0):(9,0)
(2,3):(4,0):(-119,-120)
(0,0):(1,0):(0,0)
(0,0):(2,3):(0,0)
(1,0):(0,0):(1,0)
(1,0):(1,0):(1,0)
(1,0):(2,3):(1,0)
(2,3):(0,0):(1,0)
(2,3):(1,0):(2,3)
(0,0):(0,0):(1,0)

&Re
(3,4):3
(-3,4):-3
[1,pi/2]:0

&Im
(3,4):4
(3,-4):-4
[1,pi/2]:1

&abs
(3,4):5
(-3,4):5

&arg
[2,0]:0
[-2,0]:pi

&~
(4,5):(4,-5)
(-3,4):(-3,-4)
[2,pi/2]:[2,-(pi)/2]

&<
(3,4):(1,2):0
(3,4):(3,2):0
(3,4):(3,8):1
(4,4):(5,129):1

&==
(3,4):(4,5):0
(3,4):(3,5):0
(3,4):(2,4):0
(3,4):(3,4):1

&sqrt
-9:(0,3)
(-100,0):(0,10)
(16,-30):(5,-3)

&_stringify_cartesian
(-100,0):"-100"
(0,1):"i"
(4,-3):"4-3i"
(4,0):"4"
(-4,0):"-4"
(-2,4):"-2+4i"
(-2,-1):"-2-i"

&_stringify_polar
[-1, 0]:"[1,pi]"
[1, pi/3]:"[1,pi/3]"
[6, -2*pi/3]:"[6,-2pi/3]"
[0.5, -9*pi/11]:"[0.5,-9pi/11]"
[1, 0.5]:"[1, 0.5]"

{ (4,3); [3,2]; (-3,4); (0,2); [2,1] }

|'z + ~z':'2*Re(z)'
|'z - ~z':'2*i*Im(z)'
|'z * ~z':'abs(z) * abs(z)'

{ (0.5, 0); (-0.5, 0); (2,3); [3,2]; (-3,2); (0,2); 3; 1.2; (-3, 0); (-2, -1); [2,1] }

|'(root(z, 4))[1] ** 4':'z'
|'(root(z, 5))[3] ** 5':'z'
|'(root(z, 8))[7] ** 8':'z'
|'(root(z, 8, 0)) ** 8':'z'
|'(root(z, 8, 7)) ** 8':'z'
|'abs(z)':'r'
|'acot(z)':'acotan(z)'
|'acsc(z)':'acosec(z)'
|'acsc(z)':'asin(1 / z)'
|'asec(z)':'acos(1 / z)'
|'cbrt(z)':'cbrt(r) * exp(i * t/3)'
|'cos(acos(z))':'z'
|'addsq(cos(z), sin(z))':1
|'cos(z)':'cosh(i*z)'
|'subsq(cosh(z), sinh(z))':1
|'cot(acot(z))':'z'
|'cot(z)':'1 / tan(z)'
|'cot(z)':'cotan(z)'
|'csc(acsc(z))':'z'
|'csc(z)':'1 / sin(z)'
|'csc(z)':'cosec(z)'
|'exp(log(z))':'z'
|'exp(z)':'exp(a) * exp(i * b)'
|'ln(z)':'log(z)'
|'log(exp(z))':'z'
|'log(z)':'log(r) + i*t'
|'log10(z)':'log(z) / log(10)'
|'logn(z, 2)':'log(z) / log(2)'
|'logn(z, 3)':'log(z) / log(3)'
|'sec(asec(z))':'z'
|'sec(z)':'1 / cos(z)'
|'sin(asin(z))':'z'
|'sin(i * z)':'i * sinh(z)'
|'sqrt(z) * sqrt(z)':'z'
|'sqrt(z)':'sqrt(r) * exp(i * t/2)'
|'tan(atan(z))':'z'
|'z**z':'exp(z * log(z))'

{ (1,1); [1,0.5]; (-2, -1); 2; -3; (-1,0.5); (0,0.5); 0.5; (2, 0); (-1, -2) }

|'cosh(acosh(z))':'z'
|'coth(acoth(z))':'z'
|'coth(z)':'1 / tanh(z)'
|'coth(z)':'cotanh(z)'
|'csch(acsch(z))':'z'
|'csch(z)':'1 / sinh(z)'
|'csch(z)':'cosech(z)'
|'sech(asech(z))':'z'
|'sech(z)':'1 / cosh(z)'
|'sinh(asinh(z))':'z'
|'tanh(atanh(z))':'z'

{ (0.2,-0.4); [1,0.5]; -1.2; (-1,0.5); 0.5; (1.1, 0) }

|'acos(cos(z)) ** 2':'z * z'
|'acosh(cosh(z)) ** 2':'z * z'
|'acoth(z)':'acotanh(z)'
|'acoth(z)':'atanh(1 / z)'
|'acsch(z)':'acosech(z)'
|'acsch(z)':'asinh(1 / z)'
|'asech(z)':'acosh(1 / z)'
|'asin(sin(z))':'z'
|'asinh(sinh(z))':'z'
|'atan(tan(z))':'z'
|'atanh(tanh(z))':'z'

&log
(-2.0,0):(   0.69314718055995,  3.14159265358979)
(-1.0,0):(   0               ,  3.14159265358979)
(-0.5,0):(  -0.69314718055995,  3.14159265358979)
( 0.5,0):(  -0.69314718055995,  0               )
( 1.0,0):(   0               ,  0               )
( 2.0,0):(   0.69314718055995,  0               )

&log
( 2, 3):(    1.28247467873077,  0.98279372324733)
(-2, 3):(    1.28247467873077,  2.15879893034246)
(-2,-3):(    1.28247467873077, -2.15879893034246)
( 2,-3):(    1.28247467873077, -0.98279372324733)

&sin
(-2.0,0):(  -0.90929742682568,  0               )
(-1.0,0):(  -0.84147098480790,  0               )
(-0.5,0):(  -0.47942553860420,  0               )
( 0.0,0):(   0               ,  0               )
( 0.5,0):(   0.47942553860420,  0               )
( 1.0,0):(   0.84147098480790,  0               )
( 2.0,0):(   0.90929742682568,  0               )

&sin
( 2, 3):(  9.15449914691143, -4.16890695996656)
(-2, 3):( -9.15449914691143, -4.16890695996656)
(-2,-3):( -9.15449914691143,  4.16890695996656)
( 2,-3):(  9.15449914691143,  4.16890695996656)

&cos
(-2.0,0):(  -0.41614683654714,  0               )
(-1.0,0):(   0.54030230586814,  0               )
(-0.5,0):(   0.87758256189037,  0               )
( 0.0,0):(   1               ,  0               )
( 0.5,0):(   0.87758256189037,  0               )
( 1.0,0):(   0.54030230586814,  0               )
( 2.0,0):(  -0.41614683654714,  0               )

&cos
( 2, 3):( -4.18962569096881, -9.10922789375534)
(-2, 3):( -4.18962569096881,  9.10922789375534)
(-2,-3):( -4.18962569096881, -9.10922789375534)
( 2,-3):( -4.18962569096881,  9.10922789375534)

&tan
(-2.0,0):(   2.18503986326152,  0               )
(-1.0,0):(  -1.55740772465490,  0               )
(-0.5,0):(  -0.54630248984379,  0               )
( 0.0,0):(   0               ,  0               )
( 0.5,0):(   0.54630248984379,  0               )
( 1.0,0):(   1.55740772465490,  0               )
( 2.0,0):(  -2.18503986326152,  0               )

&tan
( 2, 3):( -0.00376402564150,  1.00323862735361)
(-2, 3):(  0.00376402564150,  1.00323862735361)
(-2,-3):(  0.00376402564150, -1.00323862735361)
( 2,-3):( -0.00376402564150, -1.00323862735361)

&sec
(-2.0,0):(  -2.40299796172238,  0               )
(-1.0,0):(   1.85081571768093,  0               )
(-0.5,0):(   1.13949392732455,  0               )
( 0.0,0):(   1               ,  0               )
( 0.5,0):(   1.13949392732455,  0               )
( 1.0,0):(   1.85081571768093,  0               )
( 2.0,0):(  -2.40299796172238,  0               )

&sec
( 2, 3):( -0.04167496441114,  0.09061113719624)
(-2, 3):( -0.04167496441114, -0.09061113719624)
(-2,-3):( -0.04167496441114,  0.09061113719624)
( 2,-3):( -0.04167496441114, -0.09061113719624)

&csc
(-2.0,0):(  -1.09975017029462,  0               )
(-1.0,0):(  -1.18839510577812,  0               )
(-0.5,0):(  -2.08582964293349,  0               )
( 0.5,0):(   2.08582964293349,  0               )
( 1.0,0):(   1.18839510577812,  0               )
( 2.0,0):(   1.09975017029462,  0               )

&csc
( 2, 3):(  0.09047320975321,  0.04120098628857)
(-2, 3):( -0.09047320975321,  0.04120098628857)
(-2,-3):( -0.09047320975321, -0.04120098628857)
( 2,-3):(  0.09047320975321, -0.04120098628857)

&cot
(-2.0,0):(   0.45765755436029,  0               )
(-1.0,0):(  -0.64209261593433,  0               )
(-0.5,0):(  -1.83048772171245,  0               )
( 0.5,0):(   1.83048772171245,  0               )
( 1.0,0):(   0.64209261593433,  0               )
( 2.0,0):(  -0.45765755436029,  0               )

&cot
( 2, 3):( -0.00373971037634, -0.99675779656936)
(-2, 3):(  0.00373971037634, -0.99675779656936)
(-2,-3):(  0.00373971037634,  0.99675779656936)
( 2,-3):( -0.00373971037634,  0.99675779656936)

&asin
(-2.0,0):(  -1.57079632679490,  1.31695789692482)
(-1.0,0):(  -1.57079632679490,  0               )
(-0.5,0):(  -0.52359877559830,  0               )
( 0.0,0):(   0               ,  0               )
( 0.5,0):(   0.52359877559830,  0               )
( 1.0,0):(   1.57079632679490,  0               )
( 2.0,0):(   1.57079632679490, -1.31695789692482)

&asin
( 2, 3):(  0.57065278432110,  1.98338702991654)
(-2, 3):( -0.57065278432110,  1.98338702991654)
(-2,-3):( -0.57065278432110, -1.98338702991654)
( 2,-3):(  0.57065278432110, -1.98338702991654)

&acos
(-2.0,0):(   3.14159265358979, -1.31695789692482)
(-1.0,0):(   3.14159265358979,  0               )
(-0.5,0):(   2.09439510239320,  0               )
( 0.0,0):(   1.57079632679490,  0               )
( 0.5,0):(   1.04719755119660,  0               )
( 1.0,0):(   0               ,  0               )
( 2.0,0):(   0               ,  1.31695789692482)

&acos
( 2, 3):(  1.00014354247380, -1.98338702991654)
(-2, 3):(  2.14144911111600, -1.98338702991654)
(-2,-3):(  2.14144911111600,  1.98338702991654)
( 2,-3):(  1.00014354247380,  1.98338702991654)

&atan
(-2.0,0):(  -1.10714871779409,  0               )
(-1.0,0):(  -0.78539816339745,  0               )
(-0.5,0):(  -0.46364760900081,  0               )
( 0.0,0):(   0               ,  0               )
( 0.5,0):(   0.46364760900081,  0               )
( 1.0,0):(   0.78539816339745,  0               )
( 2.0,0):(   1.10714871779409,  0               )

&atan
( 2, 3):(  1.40992104959658,  0.22907268296854)
(-2, 3):( -1.40992104959658,  0.22907268296854)
(-2,-3):( -1.40992104959658, -0.22907268296854)
( 2,-3):(  1.40992104959658, -0.22907268296854)

&asec
(-2.0,0):(   2.09439510239320,  0               )
(-1.0,0):(   3.14159265358979,  0               )
(-0.5,0):(   3.14159265358979, -1.31695789692482)
( 0.5,0):(   0               ,  1.31695789692482)
( 1.0,0):(   0               ,  0               )
( 2.0,0):(   1.04719755119660,  0               )

&asec
( 2, 3):(  1.42041072246703,  0.23133469857397)
(-2, 3):(  1.72118193112276,  0.23133469857397)
(-2,-3):(  1.72118193112276, -0.23133469857397)
( 2,-3):(  1.42041072246703, -0.23133469857397)

&acsc
(-2.0,0):(  -0.52359877559830,  0               )
(-1.0,0):(  -1.57079632679490,  0               )
(-0.5,0):(  -1.57079632679490,  1.31695789692482)
( 0.5,0):(   1.57079632679490, -1.31695789692482)
( 1.0,0):(   1.57079632679490,  0               )
( 2.0,0):(   0.52359877559830,  0               )

&acsc
( 2, 3):(  0.15038560432786, -0.23133469857397)
(-2, 3):( -0.15038560432786, -0.23133469857397)
(-2,-3):( -0.15038560432786,  0.23133469857397)
( 2,-3):(  0.15038560432786,  0.23133469857397)

&acot
(-2.0,0):(  -0.46364760900081,  0               )
(-1.0,0):(  -0.78539816339745,  0               )
(-0.5,0):(  -1.10714871779409,  0               )
( 0.5,0):(   1.10714871779409,  0               )
( 1.0,0):(   0.78539816339745,  0               )
( 2.0,0):(   0.46364760900081,  0               )

&acot
( 2, 3):(  0.16087527719832, -0.22907268296854)
(-2, 3):( -0.16087527719832, -0.22907268296854)
(-2,-3):( -0.16087527719832,  0.22907268296854)
( 2,-3):(  0.16087527719832,  0.22907268296854)

&sinh
(-2.0,0):(  -3.62686040784702,  0               )
(-1.0,0):(  -1.17520119364380,  0               )
(-0.5,0):(  -0.52109530549375,  0               )
( 0.0,0):(   0               ,  0               )
( 0.5,0):(   0.52109530549375,  0               )
( 1.0,0):(   1.17520119364380,  0               )
( 2.0,0):(   3.62686040784702,  0               )

&sinh
( 2, 3):( -3.59056458998578,  0.53092108624852)
(-2, 3):(  3.59056458998578,  0.53092108624852)
(-2,-3):(  3.59056458998578, -0.53092108624852)
( 2,-3):( -3.59056458998578, -0.53092108624852)

&cosh
(-2.0,0):(   3.76219569108363,  0               )
(-1.0,0):(   1.54308063481524,  0               )
(-0.5,0):(   1.12762596520638,  0               )
( 0.0,0):(   1               ,  0               )
( 0.5,0):(   1.12762596520638,  0               )
( 1.0,0):(   1.54308063481524,  0               )
( 2.0,0):(   3.76219569108363,  0               )

&cosh
( 2, 3):( -3.72454550491532,  0.51182256998738)
(-2, 3):( -3.72454550491532, -0.51182256998738)
(-2,-3):( -3.72454550491532,  0.51182256998738)
( 2,-3):( -3.72454550491532, -0.51182256998738)

&tanh
(-2.0,0):(  -0.96402758007582,  0               )
(-1.0,0):(  -0.76159415595576,  0               )
(-0.5,0):(  -0.46211715726001,  0               )
( 0.0,0):(   0               ,  0               )
( 0.5,0):(   0.46211715726001,  0               )
( 1.0,0):(   0.76159415595576,  0               )
( 2.0,0):(   0.96402758007582,  0               )

&tanh
( 2, 3):(  0.96538587902213, -0.00988437503832)
(-2, 3):( -0.96538587902213, -0.00988437503832)
(-2,-3):( -0.96538587902213,  0.00988437503832)
( 2,-3):(  0.96538587902213,  0.00988437503832)

&sech
(-2.0,0):(   0.26580222883408,  0               )
(-1.0,0):(   0.64805427366389,  0               )
(-0.5,0):(   0.88681888397007,  0               )
( 0.0,0):(   1               ,  0               )
( 0.5,0):(   0.88681888397007,  0               )
( 1.0,0):(   0.64805427366389,  0               )
( 2.0,0):(   0.26580222883408,  0               )

&sech
( 2, 3):( -0.26351297515839, -0.03621163655877)
(-2, 3):( -0.26351297515839,  0.03621163655877)
(-2,-3):( -0.26351297515839, -0.03621163655877)
( 2,-3):( -0.26351297515839,  0.03621163655877)

&csch
(-2.0,0):(  -0.27572056477178,  0               )
(-1.0,0):(  -0.85091812823932,  0               )
(-0.5,0):(  -1.91903475133494,  0               )
( 0.5,0):(   1.91903475133494,  0               )
( 1.0,0):(   0.85091812823932,  0               )
( 2.0,0):(   0.27572056477178,  0               )

&csch
( 2, 3):( -0.27254866146294, -0.04030057885689)
(-2, 3):(  0.27254866146294, -0.04030057885689)
(-2,-3):(  0.27254866146294,  0.04030057885689)
( 2,-3):( -0.27254866146294,  0.04030057885689)

&coth
(-2.0,0):(  -1.03731472072755,  0               )
(-1.0,0):(  -1.31303528549933,  0               )
(-0.5,0):(  -2.16395341373865,  0               )
( 0.5,0):(   2.16395341373865,  0               )
( 1.0,0):(   1.31303528549933,  0               )
( 2.0,0):(   1.03731472072755,  0               )

&coth
( 2, 3):(  1.03574663776500,  0.01060478347034)
(-2, 3):( -1.03574663776500,  0.01060478347034)
(-2,-3):( -1.03574663776500, -0.01060478347034)
( 2,-3):(  1.03574663776500, -0.01060478347034)

&asinh
(-2.0,0):(  -1.44363547517881,  0               )
(-1.0,0):(  -0.88137358701954,  0               )
(-0.5,0):(  -0.48121182505960,  0               )
( 0.0,0):(   0               ,  0               )
( 0.5,0):(   0.48121182505960,  0               )
( 1.0,0):(   0.88137358701954,  0               )
( 2.0,0):(   1.44363547517881,  0               )

&asinh
( 2, 3):(  1.96863792579310,  0.96465850440760)
(-2, 3):( -1.96863792579310,  0.96465850440761)
(-2,-3):( -1.96863792579310, -0.96465850440761)
( 2,-3):(  1.96863792579310, -0.96465850440760)

&acosh
(-2.0,0):(   1.31695789692482,  3.14159265358979)
(-1.0,0):(   0,                 3.14159265358979)
(-0.5,0):(   0,                 2.09439510239320)
( 0.0,0):(   0,                 1.57079632679490)
( 0.5,0):(   0,                 1.04719755119660)
( 1.0,0):(   0               ,  0               )
( 2.0,0):(   1.31695789692482,  0               )

&acosh
( 2, 3):(  1.98338702991654,  1.00014354247380)
(-2, 3):(  1.98338702991653,  2.14144911111600)
(-2,-3):(  1.98338702991653, -2.14144911111600)
( 2,-3):(  1.98338702991654, -1.00014354247380)

&atanh
(-2.0,0):(  -0.54930614433405,  1.57079632679490)
(-0.5,0):(  -0.54930614433405,  0               )
( 0.0,0):(   0               ,  0               )
( 0.5,0):(   0.54930614433405,  0               )
( 2.0,0):(   0.54930614433405,  1.57079632679490)

&atanh
( 2, 3):(  0.14694666622553,  1.33897252229449)
(-2, 3):( -0.14694666622553,  1.33897252229449)
(-2,-3):( -0.14694666622553, -1.33897252229449)
( 2,-3):(  0.14694666622553, -1.33897252229449)

&asech
(-2.0,0):(   0               , 2.09439510239320)
(-1.0,0):(   0               , 3.14159265358979)
(-0.5,0):(   1.31695789692482, 3.14159265358979)
( 0.5,0):(   1.31695789692482, 0               )
( 1.0,0):(   0               , 0               )
( 2.0,0):(   0               , 1.04719755119660)

&asech
( 2, 3):(  0.23133469857397, -1.42041072246703)
(-2, 3):(  0.23133469857397, -1.72118193112276)
(-2,-3):(  0.23133469857397,  1.72118193112276)
( 2,-3):(  0.23133469857397,  1.42041072246703)

&acsch
(-2.0,0):(  -0.48121182505960, 0               )
(-1.0,0):(  -0.88137358701954, 0               )
(-0.5,0):(  -1.44363547517881, 0               )
( 0.5,0):(   1.44363547517881, 0               )
( 1.0,0):(   0.88137358701954, 0               )
( 2.0,0):(   0.48121182505960, 0               )

&acsch
( 2, 3):(  0.15735549884499, -0.22996290237721)
(-2, 3):( -0.15735549884499, -0.22996290237721)
(-2,-3):( -0.15735549884499,  0.22996290237721)
( 2,-3):(  0.15735549884499,  0.22996290237721)

&acoth
(-2.0,0):(  -0.54930614433405, 0               )
(-0.5,0):(  -0.54930614433405, 1.57079632679490)
( 0.5,0):(   0.54930614433405, 1.57079632679490)
( 2.0,0):(   0.54930614433405, 0               )

&acoth
( 2, 3):(  0.14694666622553, -0.23182380450040)
(-2, 3):( -0.14694666622553, -0.23182380450040)
(-2,-3):( -0.14694666622553,  0.23182380450040)
( 2,-3):(  0.14694666622553,  0.23182380450040)

# eof
