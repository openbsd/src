#!./perl -w

# Tests for sprintf that do not fit the format of sprintf.t.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}   

# We'll run 12 extra tests (see below) if $Q is false.
eval { my $q = pack "q", 0 };
my $Q = $@ eq '';

my $doubledouble;

# %a and %A depend on the floating point config
# This totally doesn't test non-IEEE-754 float formats.
my @hexfloat;
print "# uvsize = $Config{uvsize}\n";
print "# nvsize = $Config{nvsize}\n";
print "# nv_preserves_uv_bits = $Config{nv_preserves_uv_bits}\n";
print "# d_quad = $Config{d_quad}\n";
if ($Config{nvsize} == 8 &&
    (
     # IEEE-754 64-bit ("double precision"), the most common out there
     ($Config{uvsize} == 8 && $Config{nv_preserves_uv_bits} == 53)
     ||
     # If we have a quad we can still get the mantissa bits.
     ($Config{uvsize} == 4 && $Config{d_quad})
     )
    ) {
    @hexfloat = (
        [ '%a',       '0',       '0x0p+0' ],
        [ '%a',       '1',       '0x1p+0' ],
        [ '%a',       '1.0',     '0x1p+0' ],
        [ '%a',       '0.5',     '0x1p-1' ],
        [ '%a',       '0.25',    '0x1p-2' ],
        [ '%a',       '0.75',    '0x1.8p-1' ],
        [ '%a',       '3.14',    '0x1.91eb851eb851fp+1' ],
        [ '%a',       '-1.0',    '-0x1p+0' ],
        [ '%a',       '-3.14',   '-0x1.91eb851eb851fp+1' ],
        [ '%a',       '0.1',     '0x1.999999999999ap-4' ],
        [ '%a',       '1/7',     '0x1.2492492492492p-3' ],
        [ '%a',       'sqrt(2)', '0x1.6a09e667f3bcdp+0' ],
        [ '%a',       'exp(1)',  '0x1.5bf0a8b145769p+1' ],
        [ '%a',       '2**-10',  '0x1p-10' ],
        [ '%a',       '2**10',   '0x1p+10' ],
        [ '%a',       '1e-9',    '0x1.12e0be826d695p-30' ],
        [ '%a',       '1e9',     '0x1.dcd65p+29' ],

        [ '%#a',      '1',       '0x1.p+0' ],
        [ '%+a',      '1',       '+0x1p+0' ],
        [ '%+a',      '-1',      '-0x1p+0' ],
        [ '% a',      ' 1',      ' 0x1p+0' ],
        [ '% a',      '-1',      '-0x1p+0' ],

        [ '%+ a',     '1',       '+0x1p+0' ],
        [ '%+ a',     '-1',      '-0x1p+0' ],
        [ '% +a',     ' 1',      '+0x1p+0' ],
        [ '% +a',     '-1',      '-0x1p+0' ],

        [ '%8a',      '3.14',   '0x1.91eb851eb851fp+1' ],
        [ '%13a',     '3.14',   '0x1.91eb851eb851fp+1' ],
        [ '%20a',     '3.14',   '0x1.91eb851eb851fp+1' ],
        [ '%.4a',     '3.14',   '0x1.91ecp+1' ],
        [ '%.5a',     '3.14',   '0x1.91eb8p+1' ],
        [ '%.6a',     '3.14',   '0x1.91eb85p+1' ],
        [ '%.20a',    '3.14',   '0x1.91eb851eb851f0000000p+1' ],
        [ '%20.10a',  '3.14',   '   0x1.91eb851eb8p+1' ],
        [ '%20.15a',  '3.14',   '0x1.91eb851eb851f00p+1' ],
        [ '% 20.10a', '3.14',   '   0x1.91eb851eb8p+1' ],
        [ '%020.10a', '3.14',   '0x0001.91eb851eb8p+1' ],

        [ '%.13a',    '1',   '0x1.0000000000000p+0' ],
        [ '%.13a',    '-1',  '-0x1.0000000000000p+0' ],
        [ '%.13a',    '0',   '0x0.0000000000000p+0' ],

        [ '%30a',  '3.14',   '          0x1.91eb851eb851fp+1' ],
        [ '%-30a', '3.14',   '0x1.91eb851eb851fp+1          ' ],
        [ '%030a',  '3.14',  '0x00000000001.91eb851eb851fp+1' ],
        [ '%-030a', '3.14',  '0x1.91eb851eb851fp+1          ' ],

        [ '%.40a',  '3.14',
          '0x1.91eb851eb851f000000000000000000000000000p+1' ],

        [ '%A',       '3.14',   '0X1.91EB851EB851FP+1' ],
        );
} elsif (($Config{nvsize} == 16 || $Config{nvsize} == 12) &&
         # 80-bit ("extended precision") long double, pack F is the NV
         # cd cc cc cc cc cc cc cc fb bf 00 00 00 00 00 00
         # cd cc cc cc cc cc cc cc fb bf 00 00
         (pack("F", 0.1) =~ /^\xCD/ ||  # LE
          pack("F", 0.1) =~ /\xCD$/)) { # BE (if this ever happens)
    @hexfloat = (
        [ '%a',       '0',       '0x0p+0' ],
        [ '%a',       '1',       '0x8p-3' ],
        [ '%a',       '1.0',     '0x8p-3' ],
        [ '%a',       '0.5',     '0x8p-4' ],
        [ '%a',       '0.25',    '0x8p-5' ],
        [ '%a',       '0.75',    '0xcp-4' ],
        [ '%a',       '3.14',    '0xc.8f5c28f5c28f5c3p-2' ],
        [ '%a',       '-1.0',    '-0x8p-3' ],
        [ '%a',       '-3.14',   '-0xc.8f5c28f5c28f5c3p-2' ],
        [ '%a',       '0.1',     '0xc.ccccccccccccccdp-7' ],
        [ '%a',       '1/7',     '0x9.249249249249249p-6' ],
        [ '%a',       'sqrt(2)', '0xb.504f333f9de6484p-3' ],
        [ '%a',       'exp(1)',  '0xa.df85458a2bb4a9bp-2' ],
        [ '%a',       '2**-10',  '0x8p-13' ],
        [ '%a',       '2**10',   '0x8p+7' ],
        [ '%a',       '1e-9',    '0x8.9705f4136b4a597p-33' ],
        [ '%a',       '1e9',     '0xe.e6b28p+26' ],

        [ '%#a',      '1',       '0x8.p-3' ],
        [ '%+a',      '1',       '+0x8p-3' ],
        [ '%+a',      '-1',      '-0x8p-3' ],
        [ '% a',      ' 1',      ' 0x8p-3' ],
        [ '% a',      '-1',      '-0x8p-3' ],

        [ '%+ a',     '1',       '+0x8p-3' ],
        [ '%+ a',     '-1',      '-0x8p-3' ],
        [ '% +a',     ' 1',      '+0x8p-3' ],
        [ '% +a',     '-1',      '-0x8p-3' ],

        [ '%8a',      '3.14',    '0xc.8f5c28f5c28f5c3p-2' ],
        [ '%13a',     '3.14',    '0xc.8f5c28f5c28f5c3p-2' ],
        [ '%20a',     '3.14',    '0xc.8f5c28f5c28f5c3p-2' ],
        [ '%.4a',     '3.14',    '0xc.8f5cp-2' ],
        [ '%.5a',     '3.14',    '0xc.8f5c3p-2' ],
        [ '%.6a',     '3.14',    '0xc.8f5c29p-2' ],
        [ '%.20a',    '3.14',    '0xc.8f5c28f5c28f5c300000p-2' ],
        [ '%20.10a',  '3.14',    '   0xc.8f5c28f5c3p-2' ],
        [ '%20.15a',  '3.14',    '0xc.8f5c28f5c28f5c3p-2' ],
        [ '% 20.10a', '3.14',    '   0xc.8f5c28f5c3p-2' ],
        [ '%020.10a', '3.14',    '0x000c.8f5c28f5c3p-2' ],

        [ '%30a',  '3.14',   '        0xc.8f5c28f5c28f5c3p-2' ],
        [ '%-30a', '3.14',   '0xc.8f5c28f5c28f5c3p-2        ' ],
        [ '%030a',  '3.14',  '0x00000000c.8f5c28f5c28f5c3p-2' ],
        [ '%-030a', '3.14',  '0xc.8f5c28f5c28f5c3p-2        ' ],

        [ '%.40a',  '3.14',
          '0xc.8f5c28f5c28f5c30000000000000000000000000p-2' ],

        [ '%A',       '3.14',    '0XC.8F5C28F5C28F5C3P-2' ],
        );
} elsif (
    # IEEE 754 128-bit ("quadruple precision"), e.g. IA-64 (Itanium) in VMS
    $Config{nvsize} == 16 &&
    # 9a 99 99 99 99 99 99 99 99 99 99 99 99 99 fb 3f (LE), pack F is the NV
    (pack("F", 0.1) =~ /^\x9A\x99{6}/ ||  # LE
     pack("F", 0.1) =~ /\x99{6}\x9A$/)    # BE
    ) {
    @hexfloat = (
	[ '%a', '0',       '0x0p+0' ],
	[ '%a', '1',       '0x1p+0' ],
	[ '%a', '1.0',     '0x1p+0' ],
	[ '%a', '0.5',     '0x1p-1' ],
	[ '%a', '0.25',    '0x1p-2' ],
	[ '%a', '0.75',    '0x1.8p-1' ],
	[ '%a', '3.14',    '0x1.91eb851eb851eb851eb851eb851fp+1' ],
	[ '%a', '-1',      '-0x1p+0' ],
	[ '%a', '-3.14',   '-0x1.91eb851eb851eb851eb851eb851fp+1' ],
	[ '%a', '0.1',     '0x1.999999999999999999999999999ap-4' ],
	[ '%a', '1/7',     '0x1.2492492492492492492492492492p-3' ],
	[ '%a', 'sqrt(2)', '0x1.6a09e667f3bcc908b2fb1366ea95p+0' ],
	[ '%a', 'exp(1)',  '0x1.5bf0a8b1457695355fb8ac404e7ap+1' ],
	[ '%a', '2**-10',  '0x1p-10' ],
	[ '%a', '2**10',   '0x1p+10' ],
	[ '%a', '1e-09',   '0x1.12e0be826d694b2e62d01511f12ap-30' ],
	[ '%a', '1e9',     '0x1.dcd65p+29' ],

	[ '%#a', '1',      '0x1.p+0' ],
	[ '%+a', '1',      '+0x1p+0' ],
	[ '%+a', '-1',     '-0x1p+0' ],
	[ '% a', '1',      ' 0x1p+0' ],
	[ '% a', '-1',     '-0x1p+0' ],

        [ '%+ a', '1',     '+0x1p+0' ],
        [ '%+ a', '-1',    '-0x1p+0' ],
        [ '% +a', ' 1',    '+0x1p+0' ],
        [ '% +a', '-1',    '-0x1p+0' ],

	[ '%8a',      '3.14', '0x1.91eb851eb851eb851eb851eb851fp+1' ],
	[ '%13a',     '3.14', '0x1.91eb851eb851eb851eb851eb851fp+1' ],
	[ '%20a',     '3.14', '0x1.91eb851eb851eb851eb851eb851fp+1' ],
	[ '%.4a',     '3.14', '0x1.91ecp+1' ],
	[ '%.5a',     '3.14', '0x1.91eb8p+1' ],
	[ '%.6a',     '3.14', '0x1.91eb85p+1' ],
	[ '%.20a',    '3.14', '0x1.91eb851eb851eb851eb8p+1' ],
	[ '%20.10a',  '3.14', '   0x1.91eb851eb8p+1' ],
	[ '%20.15a',  '3.14', '0x1.91eb851eb851eb8p+1' ],
	[ '% 20.10a', '3.14', '   0x1.91eb851eb8p+1' ],
	[ '%020.10a', '3.14', '0x0001.91eb851eb8p+1' ],

	[ '%30a',     '3.14', '0x1.91eb851eb851eb851eb851eb851fp+1' ],
	[ '%-30a',    '3.14', '0x1.91eb851eb851eb851eb851eb851fp+1' ],
	[ '%030a',    '3.14', '0x1.91eb851eb851eb851eb851eb851fp+1' ],
	[ '%-030a',   '3.14', '0x1.91eb851eb851eb851eb851eb851fp+1' ],

        [ '%.40a',  '3.14',
          '0x1.91eb851eb851eb851eb851eb851f000000000000p+1' ],

	[ '%A',       '3.14', '0X1.91EB851EB851EB851EB851EB851FP+1' ],
        );
} elsif (
    # "double-double", two 64-bit doubles end to end
    $Config{nvsize} == 16 &&
    # bf b9 99 99 99 99 99 9a bc 59 99 99 99 99 99 9a (BE), pack F is the NV
    (pack("F", 0.1) =~ /^\x9A\x99{5}\x59\xBC/ ||  # LE
     pack("F", 0.1) =~ /\xBC\x59\x99{5}\x9A$/)    # BE
    ) {
    $doubledouble = 1;
    @hexfloat = (
	[ '%a', '0',       '0x0p+0' ],
	[ '%a', '1',       '0x1p+0' ],
	[ '%a', '1.0',     '0x1p+0' ],
	[ '%a', '0.5',     '0x1p-1' ],
	[ '%a', '0.25',    '0x1p-2' ],
	[ '%a', '0.75',    '0x1.8p-1' ],
	[ '%a', '3.14',    '0x1.91eb851eb851eb851eb851eb85p+1' ],
	[ '%a', '-1',      '-0x0p+0' ],
	[ '%a', '-3.14',   '-0x1.91eb851eb851eb851eb851eb85p+1' ],
	[ '%a', '0.1',     '0x1.999999999999999999999999998p-4' ],
	[ '%a', '1/7',     '0x1.249249249249249249249249248p-3' ],
	[ '%a', 'sqrt(2)', '0x1.6a09e667f3bcc908b2fb1366ea8p+0' ],
	[ '%a', 'exp(1)',  '0x1.5bf0a8b1457695355fb8ac404e8p+1' ],
	[ '%a', '2**-10',  '0x1p-10' ],
	[ '%a', '2**10',   '0x1p+10' ],
	[ '%a', '1e-09',   '0x1.12e0be826d694b2e62d01511f14p-30' ],
	[ '%a', '1e9',     '0x1.dcd65p+29' ],

	[ '%#a', '1',      '0x1.p+0' ],
	[ '%+a', '1',      '+0x1p+0' ],
	[ '%+a', '-1',     '-0x1p+0' ],
	[ '% a', '1',      ' 0x1p+0' ],
	[ '% a', '-1',     '-0x1p+0' ],

	[ '%8a',      '3.14', '0x1.91eb851eb851eb851eb851eb85p+1' ],
	[ '%13a',     '3.14', '0x1.91eb851eb851eb851eb851eb85p+1' ],
	[ '%20a',     '3.14', '0x1.91eb851eb851eb851eb851eb85p+1' ],
	[ '%.4a',     '3.14', '0x1.91ecp+1' ],
	[ '%.5a',     '3.14', '0x1.91eb8p+1' ],
	[ '%.6a',     '3.14', '0x1.91eb85p+1' ],
        [ '%.20a',    '3.14',   '0x1.91eb851eb851eb851eb8p+1' ],
	[ '%20.10a',  '3.14', '   0x1.91eb851eb8p+1' ],
        [ '%20.15a',  '3.14',   '0x1.91eb851eb851eb8p+1' ],
	[ '% 20.10a', '3.14', '   0x1.91eb851eb8p+1' ],
	[ '%020.10a', '3.14', '0x0001.91eb851eb8p+1' ],

        [ '%30a',  '3.14',   '0x1.91eb851eb851eb851eb851eb85p+1' ],
        [ '%-30a', '3.14',   '0x1.91eb851eb851eb851eb851eb85p+1' ],
        [ '%030a',  '3.14',  '0x1.91eb851eb851eb851eb851eb85p+1' ],
        [ '%-030a', '3.14',  '0x1.91eb851eb851eb851eb851eb85p+1' ],

        [ '%.40a',  '3.14',
          '0x1.91eb851eb851eb851eb851eb8500000000000000p+1' ],

	[ '%A',       '3.14', '0X1.91EB851EB851EB851EB851EB85P+1' ],
        );
} else {
    print "# no hexfloat tests\n";
}

plan tests => 1408 + ($Q ? 0 : 12) + @hexfloat + 12;

use strict;
use Config;

is(
    sprintf("%.40g ",0.01),
    sprintf("%.40g", 0.01)." ",
    q(the sprintf "%.<number>g" optimization)
);
is(
    sprintf("%.40f ",0.01),
    sprintf("%.40f", 0.01)." ",
    q(the sprintf "%.<number>f" optimization)
);

# cases of $i > 1 are against [perl #39126]
for my $i (1, 5, 10, 20, 50, 100) {
    chop(my $utf8_format = "%-*s\x{100}");
    my $string = "\xB4"x$i;        # latin1 ACUTE or ebcdic COPYRIGHT
    my $expect = $string."  "x$i;  # followed by 2*$i spaces
    is(sprintf($utf8_format, 3*$i, $string), $expect,
       "width calculation under utf8 upgrade, length=$i");
}

# check simultaneous width & precision with wide characters
for my $i (1, 3, 5, 10) {
    my $string = "\x{0410}"x($i+10);   # cyrillic capital A
    my $expect = "\x{0410}"x$i;        # cut down to exactly $i characters
    my $format = "%$i.${i}s";
    is(sprintf($format, $string), $expect,
       "width & precision interplay with utf8 strings, length=$i");
}

# Used to mangle PL_sv_undef
fresh_perl_like(
    'print sprintf "xxx%n\n"; print undef',
    qr/Modification of a read-only value attempted at - line 1\./,
    { switches => [ '-w' ] },
    q(%n should not be able to modify read-only constants),
);

# check overflows
for (int(~0/2+1), ~0, "9999999999999999999") {
    is(eval {sprintf "%${_}d", 0}, undef, "no sprintf result expected %${_}d");
    like($@, qr/^Integer overflow in format string for sprintf /, "overflow in sprintf");
    is(eval {printf "%${_}d\n", 0}, undef, "no printf result expected %${_}d");
    like($@, qr/^Integer overflow in format string for printf /, "overflow in printf");
}

# check %NNN$ for range bounds
{
    my ($warn, $bad) = (0,0);
    local $SIG{__WARN__} = sub {
	if ($_[0] =~ /missing argument/i) {
	    $warn++
	}
	else {
	    $bad++
	}
    };

    my $fmt = join('', map("%$_\$s%" . ((1 << 31)-$_) . '$s', 1..20));
    my $result = sprintf $fmt, qw(a b c d);
    is($result, "abcd", "only four valid values in $fmt");
    is($warn, 36, "expected warnings");
    is($bad,   0, "unexpected warnings");
}

# Tests for "missing argument" and "redundant argument" warnings
{
    my ($warn_missing, $warn_redundant, $warn_bad) = (0,0,0);
    local $SIG{__WARN__} = sub {
	if ($_[0] =~ /missing argument/i) {
	    $warn_missing++
	}
	elsif ($_[0] =~ /redundant argument/i) {
	    $warn_redundant++
	}
	else {
	    $warn_bad++
	}
    };

    my @tests = (
	# The "", "%s", and "%-p" formats have special-case handling
	# in sv.c
	{
	    fmt	 => "",
	    args => [ qw( x ) ],
	    res	 => "",
	    m	 => 0,
	    r	 => 1,
	},
	{
	    fmt	 => "%s",
	    args => [ qw( x y ) ],
	    res	 => "x",
	    m	 => 0,
	    r	 => 1,
	},
	{
	    fmt	 => "%-p",
	    args => [ qw( x y ) ],
	    res	 => qr/^[0-9a-f]+$/as,
	    m	 => 0,
	    r	 => 1,
	},
	# Other non-specialcased patterns
	{
	    fmt	 => "%s : %s",
	    args => [ qw( a b c ) ],
	    res	 => "a : b",
	    m	 => 0,
	    r	 => 1,
	},
	{
	    fmt	 => "%s : %s : %s",
	    args => [ qw( a b c d e ) ],
	    res	 => "a : b : c",
	    m	 => 0,
	    # Note how we'll only warn about redundant arguments once,
	    # even though both "d" and "e" are redundant...
	    r	 => 1,
	},
	{
	    fmt	 => "%s : %s : %s",
	    args => [ ],
	    res	 => " :  : ",
	    # ...But when arguments are missing we'll warn about every
	    # missing argument. This difference between the two
	    # warnings is a feature.
	    m	 => 3,
	    r	 => 0,
	},

	# Tests for format parameter indexes.
	#
	# Deciding what to do about these is a bit tricky, and so is
	# "correctly" warning about missing arguments on them.
	#
	# Should we warn if you supply 4 arguments but only use
	# argument 1,3 & 4? Or only if you supply 5 arguments and your
	# highest used argument is 4?
	#
	# For some uses of this printf feature (e.g. i18n systems)
	# it's a always a logic error to not print out every provided
	# argument, but for some other uses skipping some might be a
	# feature (although you could argue that then printf should be
	# called as e.g:
	#
	#     printf q[%1$s %3$s], x(), undef, z();
	#
	# Instead of:
	#
	#    printf q[%1$s %3$s], x(), y(), z();
	#
	# Since calling the (possibly expensive) y() function is
	# completely redundant there.
	#
	# We deal with all these potential problems by not even
	# trying. If the pattern contains any format parameter indexes
	# whatsoever we'll never warn about redundant arguments.
	{
	    fmt	 => '%1$s : %2$s',
	    args => [ qw( x y z ) ],
	    res	 => "x : y",
	    m	 => 0,
	    r	 => 0,
	},
	{
	    fmt	 => '%2$s : %4$s : %5$s',
	    args => [ qw( a b c d )],
	    res	 => "b : d : ",
	    m	 => 1,
	    r	 => 0,
	},
	{
	    fmt	 => '%s : %1$s : %s',
	    args => [ qw( x y z ) ],
	    res	 => "x : x : y",
	    m	 => 0,
	    r	 => 0,
	},

    );

    for my $i (0..$#tests) {
	my $test = $tests[$i];
	my $result = sprintf $test->{fmt}, @{$test->{args}};

	my $prefix = "For format '$test->{fmt}' and arguments/result '@{$test->{args}}'/'$result'";
	if (ref $test->{res} eq 'Regexp') {
	    like($result, $test->{res}, "$prefix got the right result");
	} else {
	    is($result, $test->{res}, "$prefix got the right result");
	}
	is($warn_missing, $test->{m}, "$prefix got '$test->{m}' 'missing argument' warnings");
	is($warn_redundant, $test->{r}, "$prefix got '$test->{r}' 'redundant argument' warnings");
	is($warn_bad, 0, "$prefix No unknown warnings");

	($warn_missing, $warn_redundant, $warn_bad) = (0,0,0);
    }
}

{
    foreach my $ord (0 .. 255) {
	my $bad = 0;
	local $SIG{__WARN__} = sub {
	    if ($_[0] !~ /^Invalid conversion in sprintf/) {
		warn $_[0];
		$bad++;
	    }
	};
	my $r = eval {sprintf '%v' . chr $ord};
	is ($bad, 0, "pattern '%v' . chr $ord");
    }
}

sub mysprintf_int_flags {
    my ($fmt, $num) = @_;
    die "wrong format $fmt" if $fmt !~ /^%([-+ 0]+)([1-9][0-9]*)d\z/;
    my $flag  = $1;
    my $width = $2;
    my $sign  = $num < 0 ? '-' :
		$flag =~ /\+/ ? '+' :
		$flag =~ /\ / ? ' ' :
		'';
    my $abs   = abs($num);
    my $padlen = $width - length($sign.$abs);
    return
	$flag =~ /0/ && $flag !~ /-/ # do zero padding
	    ? $sign . '0' x $padlen . $abs
	    : $flag =~ /-/ # left or right
		? $sign . $abs . ' ' x $padlen
		: ' ' x $padlen . $sign . $abs;
}

# Whole tests for "%4d" with 2 to 4 flags;
# total counts: 3 * (4**2 + 4**3 + 4**4) == 1008

my @flags = ("-", "+", " ", "0");
for my $num (0, -1, 1) {
    for my $f1 (@flags) {
	for my $f2 (@flags) {
	    for my $f3 ('', @flags) { # '' for doubled flags
		my $flag = $f1.$f2.$f3;
		my $width = 4;
		my $fmt   = '%'."${flag}${width}d";
		my $result = sprintf($fmt, $num);
		my $expect = mysprintf_int_flags($fmt, $num);
		is($result, $expect, qq/sprintf("$fmt",$num)/);

	        next if $f3 eq '';

		for my $f4 (@flags) { # quadrupled flags
		    my $flag = $f1.$f2.$f3.$f4;
		    my $fmt   = '%'."${flag}${width}d";
		    my $result = sprintf($fmt, $num);
		    my $expect = mysprintf_int_flags($fmt, $num);
		    is($result, $expect, qq/sprintf("$fmt",$num)/);
		}
	    }
	}
    }
}

# test that %f doesn't panic with +Inf, -Inf, NaN [perl #45383]
foreach my $n (2**1e100, -2**1e100, 2**1e100/2**1e100) { # +Inf, -Inf, NaN
    eval { my $f = sprintf("%f", $n); };
    is $@, "", "sprintf(\"%f\", $n)";
}

# test %ll formats with and without HAS_QUAD
my @tests = (
  [ '%lld' => [qw( 4294967296 -100000000000000 )] ],
  [ '%lli' => [qw( 4294967296 -100000000000000 )] ],
  [ '%llu' => [qw( 4294967296  100000000000000 )] ],
  [ '%Ld'  => [qw( 4294967296 -100000000000000 )] ],
  [ '%Li'  => [qw( 4294967296 -100000000000000 )] ],
  [ '%Lu'  => [qw( 4294967296  100000000000000 )] ],
);

for my $t (@tests) {
  my($fmt, $nums) = @$t;
  for my $num (@$nums) {
    my $w = '';
    local $SIG{__WARN__} = sub { $w .= shift };
    my $sprintf_got = sprintf($fmt, $num);
    if ($Q) {
      is($sprintf_got, $num, "quad: $fmt -> $num");
      is($w, '', "no warnings for: $fmt -> $num");
    } else {
      is($sprintf_got, $fmt, "quad unsupported: $fmt -> $fmt");
      like($w, qr/Invalid conversion in sprintf: "$fmt"/, "got warning about invalid conversion from fmt : $fmt");
      like($w, qr/Redundant argument in sprintf/, "got warning about redundant argument in sprintf from fmt : $fmt");
    }
  }
}

# Check unicode vs byte length
for my $width (1,2,3,4,5,6,7) {
    for my $precis (1,2,3,4,5,6,7) {
        my $v = "\x{20ac}\x{20ac}";
        my $format = "%" . $width . "." . $precis . "s";
        my $chars = ($precis > 2 ? 2 : $precis);
        my $space = ($width < 2 ? 0 : $width - $chars);
        fresh_perl_is(
            'my $v = "\x{20ac}\x{20ac}"; my $x = sprintf "'.$format.'", $v; $x =~ /^(\s*)(\S*)$/; print "$_" for map {length} $1, $2',
            "$space$chars",
            {},
            q(sprintf ").$format.q(", "\x{20ac}\x{20ac}"),
        );
    }
}

# Overload count
package o {
    use overload
        '""', sub { ++our $count; $_[0][0]; },
        '0+', sub { ++our $numcount; $_[0][1]; }
}
my $o = bless ["\x{100}",42], o::;
() = sprintf "%1s", $o;
is $o::count, '1', 'sprinf %1s overload count';
$o::count = 0;
() = sprintf "%.1s", $o;
is $o::count, '1', 'sprinf %.1s overload count';
$o::count = 0;
() = sprintf "%d", $o;
is $o::count,    0, 'sprintf %d string overload count is 0';
is $o::numcount, 1, 'sprintf %d number overload count is 1';

my $ppc_linux = $Config{archname} =~ /^(?:ppc|power(?:pc)?)(?:64)?-linux/;
my $irix_ld   = $Config{archname} =~ /^IP\d+-irix-ld$/;

for my $t (@hexfloat) {
    my ($format, $arg, $expected) = @$t;
    $arg = eval $arg;
    my $result = sprintf($format, $arg);
    my $ok = $result eq $expected;
    # For certain platforms (all of which are currently double-double,
    # but different implementations, GNU vs vendor, two different archs
    # (ppc and mips), and two different libm interfaces) we have some
    # bits-in-the-last-hexdigit differences.
    # Patch them up as TODOs instead of deadly errors.
    if ($doubledouble && $ppc_linux && $arg =~ /^2.71828/) {
        # gets  '0x1.5bf0a8b1457695355fb8ac404ecp+1'
        # wants '0x1.5bf0a8b1457695355fb8ac404e8p+1'
        local $::TODO = "$Config{archname} exp(1)";
        ok($ok, "'$format' '$arg' -> '$result' cf '$expected'");
        next;
    }
    if ($doubledouble && $irix_ld && $arg =~ /^1.41421/) {
        # gets  '0x1.6a09e667f3bcc908b2fb1366eacp+0'
        # wants '0x1.6a09e667f3bcc908b2fb1366ea8p+0'
        local $::TODO = "$Config{archname} sqrt(2)";
        ok($ok, "'$format' '$arg' -> '$result' cf '$expected'");
        next;
    }
    unless ($ok) {
        # It seems that there can be difference in the last bits:
        # [perl #122578]
        #      got "0x1.5bf0a8b14576ap+1"
        # expected "0x1.5bf0a8b145769p+1"
        # (Android on ARM)
        #
        # Exact cause unknown but suspecting different fp rounding modes,
        # (towards zero? towards +inf? towards -inf?) about which Perl
        # is blissfully unaware.
        #
        # Try extracting one (or sometimes two) last mantissa
        # hexdigits, and see if they differ in value by one.
        my ($rh, $eh) = ($result, $expected);
        sub extract_prefix {
            ($_[0] =~ s/(-?0x[0-9a-fA-F]+\.)//) && return $1;
        }
        my $rp = extract_prefix($rh);
        my $ep = extract_prefix($eh);
        print "# rp = $rp, ep = $ep (rh $rh, eh $eh)\n";
        if ($rp eq $ep) { # If prefixes match.
            sub extract_exponent {
                ($_[0] =~ s/([pP][+-]?\d+)//) && return $1;
            }
            my $re = extract_exponent($rh);
            my $ee = extract_exponent($eh);
            print "# re = $re, ee = $ee (rh $rh, eh $eh)\n";
            if ($re eq $ee) { # If exponents match.
                # Remove the common prefix of the mantissa bits.
                my $la = length($rh);
                my $lb = length($eh);
                my $i;
                for ($i = 0; $i < $la && $i < $lb; $i++) {
                    last if substr($rh, $i, 1) ne substr($eh, $i, 1);
                }
                $rh = substr($rh, $i);
                $eh = substr($eh, $i);
                print "# (rh $rh, eh $eh)\n";
                if ($rh ne $eh) {
                    # If necessary, pad the shorter one on the right
                    # with one zero (for example "...1f" vs "...2",
                    # we want to compare "1f" to "20").
                    if (length $rh < length $eh) {
                        $rh .= '0';
                    } elsif (length $eh < length $rh) {
                        $eh .= '0';
                    }
                    print "# (rh $rh, eh $eh)\n";
                    if (length $eh == length $rh) {
                        if (abs(hex($eh) - hex($rh)) == 1) {
                            $ok = 1;
                        }
                    }
                }
            }
        }
    }
    ok($ok, "'$format' '$arg' -> '$result' cf '$expected'");
}

# double-double long double %a special testing.
SKIP: {
    skip("uselongdouble=" . ($Config{uselongdouble} ? 'define' : 'undef')
         . " longdblkind=$Config{longdblkind} os=$^O", 6)
        unless ($Config{uselongdouble} &&
                ($Config{longdblkind} == 5 ||
                 $Config{longdblkind} == 6)
                # Gating on 'linux' (ppc) here is due to the differing
                # double-double implementations: other (also big-endian)
                # double-double platforms (e.g. AIX on ppc or IRIX on mips)
                # do not behave similarly.
                && $^O eq 'linux'
                );
    # [rt.perl.org 125633]
    like(sprintf("%La\n", (2**1020) + (2**-1072)),
         qr/^0x1.0{522}1p\+1020$/);
    like(sprintf("%La\n", (2**1021) + (2**-1072)),
         qr/^0x1.0{523}8p\+1021$/);
    like(sprintf("%La\n", (2**1022) + (2**-1072)),
         qr/^0x1.0{523}4p\+1022$/);
    like(sprintf("%La\n", (2**1023) + (2**-1072)),
         qr/^0x1.0{523}2p\+1023$/);
    like(sprintf("%La\n", (2**1023) + (2**-1073)),
         qr/^0x1.0{523}1p\+1023$/);
    like(sprintf("%La\n", (2**1023) + (2**-1074)),
         qr/^0x1.0{524}8p\+1023$/);
}

SKIP: {
    skip("negative zero not available\n", 3)
        unless sprintf('%+f', -0.0) =~ /^-0/;
    is(sprintf("%a", -0.0), "-0x0p+0", "negative zero");
    is(sprintf("%+a", -0.0), "-0x0p+0", "negative zero");
    is(sprintf("%.13a", -0.0), "-0x0.0000000000000p+0", "negative zero");
}

SKIP: {
    # [perl #127183] Non-canonical hexadecimal floats are parsed prematurely

    skip("nv_preserves_uv_bits is $Config{nv_preserves_uv_bits}, not 53", 3)
        unless $Config{nv_preserves_uv_bits} == 53;

    {
        # The 0x0.b17217f7d1cf78p0 is the original LHS value
        # from [perl #127183], its bits are 0x162e42fefa39ef << 3,
        # resulting in a non-canonical form of hexfp, where the most
        # significant bit is zero, instead of one.
        is(sprintf("%a", 0x0.b17217f7d1cf78p0 - 0x1.62e42fefa39efp-1),
           "0x0p+0",
           "non-canonical form [perl #127183]");
    }

    {
        no warnings 'overflow';  # Not the point here.

        # The 0x058b90bfbe8e7bc is 0x162e42fefa39ef << 2,
        # the 0x02c5c85fdf473de is 0x162e42fefa39ef << 1,
        # see above.
        is(sprintf("%a", 0x0.58b90bfbe8e7bcp1 - 0x1.62e42fefa39efp-1),
           "0x0p+0",
           "non-canonical form");

        is(sprintf("%a", 0x0.2c5c85fdf473dep2 - 0x1.62e42fefa39efp-1),
           "0x0p+0",
           "non-canonical form");
    }
}
