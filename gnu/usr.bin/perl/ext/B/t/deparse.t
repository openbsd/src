#!./perl

BEGIN {
    if ($ENV{PERL_CORE}){
	chdir('t') if -d 't';
	if ($^O eq 'MacOS') {
	    @INC = qw(: ::lib ::macos:lib);
	} else {
	    @INC = '.';
	    push @INC, '../lib';
	}
    } else {
	unshift @INC, 't';
    }
    require Config;
    if (($Config::Config{'extensions'} !~ /\bB\b/) ){
        print "1..0 # Skip -- Perl configured without B module\n";
        exit 0;
    }
}

use warnings;
use strict;
BEGIN {
    # BEGIN block is acutally a subroutine :-)
    return unless $] > 5.009;
    require feature;
    feature->import(':5.10');
}
use Test::More tests => 70;
use Config ();

use B::Deparse;
my $deparse = B::Deparse->new();
ok($deparse);

# Tell B::Deparse about our ambient pragmas
{ my ($hint_bits, $warning_bits, $hinthash);
 BEGIN { ($hint_bits, $warning_bits, $hinthash) = ($^H, ${^WARNING_BITS}, \%^H); }
 $deparse->ambient_pragmas (
     hint_bits    => $hint_bits,
     warning_bits => $warning_bits,
     '$['         => 0 + $[,
     '%^H'	  => $hinthash,
 );
}

$/ = "\n####\n";
while (<DATA>) {
    chomp;
    # This code is pinched from the t/lib/common.pl for TODO.
    # It's not clear how to avoid duplication
    # Now tweaked a bit to do skip or todo
    my %reason;
    foreach my $what (qw(skip todo)) {
	s/^#\s*\U$what\E\s*(.*)\n//m and $reason{$what} = $1;
	# If the SKIP reason starts ? then it's taken as a code snippet to
	# evaluate. This provides the flexibility to have conditional SKIPs
	if ($reason{$what} && $reason{$what} =~ s/^\?//) {
	    my $temp = eval $reason{$what};
	    if ($@) {
		die "# In \U$what\E code reason:\n# $reason{$what}\n$@";
	    }
	    $reason{$what} = $temp;
	}
    }

    s/^\s*#\s*(.*)$//mg;
    my ($num, $testname) = $1 =~ m/(\d+)\s*(.*)/;

    if ($reason{skip}) {
	# Like this to avoid needing a label SKIP:
       Test::More->builder->skip($reason{skip});
	next;
    }

    my ($input, $expected);
    if (/(.*)\n>>>>\n(.*)/s) {
	($input, $expected) = ($1, $2);
    }
    else {
	($input, $expected) = ($_, $_);
    }

    my $coderef = eval "sub {$input}";

    if ($@) {
	diag("$num deparsed: $@");
	ok(0, $testname);
    }
    else {
	my $deparsed = $deparse->coderef2text( $coderef );
	my $regex = $expected;
	$regex =~ s/(\S+)/\Q$1/g;
	$regex =~ s/\s+/\\s+/g;
	$regex = '^\{\s*' . $regex . '\s*\}$';

	local $::TODO = $reason{todo};
        like($deparsed, qr/$regex/, $testname);
    }
}

use constant 'c', 'stuff';
is((eval "sub ".$deparse->coderef2text(\&c))->(), 'stuff');

my $a = 0;
is("{\n    (-1) ** \$a;\n}", $deparse->coderef2text(sub{(-1) ** $a }));

use constant cr => ['hello'];
my $string = "sub " . $deparse->coderef2text(\&cr);
my $val = (eval $string)->() or diag $string;
is(ref($val), 'ARRAY');
is($val->[0], 'hello');

my $Is_VMS = $^O eq 'VMS';
my $Is_MacOS = $^O eq 'MacOS';

my $path = join " ", map { qq["-I$_"] } @INC;
$path .= " -MMac::err=unix" if $Is_MacOS;
my $redir = $Is_MacOS ? "" : "2>&1";

$a = `$^X $path "-MO=Deparse" -anlwi.bak -e 1 $redir`;
$a =~ s/-e syntax OK\n//g;
$a =~ s/.*possible typo.*\n//;	   # Remove warning line
$a =~ s{\\340\\242}{\\s} if (ord("\\") == 224); # EBCDIC, cp 1047 or 037
$a =~ s{\\274\\242}{\\s} if (ord("\\") == 188); # $^O eq 'posix-bc'
$b = <<'EOF';
BEGIN { $^I = ".bak"; }
BEGIN { $^W = 1; }
BEGIN { $/ = "\n"; $\ = "\n"; }
LINE: while (defined($_ = <ARGV>)) {
    chomp $_;
    our(@F) = split(' ', $_, 0);
    '???';
}
EOF
$b =~ s/(LINE:)/sub BEGIN {
    'MacPerl'->bootstrap;
    'OSA'->bootstrap;
    'XL'->bootstrap;
}
$1/ if $Is_MacOS;
is($a, $b);

#Re: perlbug #35857, patch #24505
#handle warnings::register-ed packages properly.
package B::Deparse::Wrapper;
use strict;
use warnings;
use warnings::register;
sub getcode {
   my $deparser = B::Deparse->new();
   return $deparser->coderef2text(shift);
}

package Moo;
use overload '0+' => sub { 42 };

package main;
use strict;
use warnings;
use constant GLIPP => 'glipp';
use constant PI => 4;
use constant OVERLOADED_NUMIFICATION => bless({}, 'Moo');
use Fcntl qw/O_TRUNC O_APPEND O_EXCL/;
BEGIN { delete $::Fcntl::{O_APPEND}; }
use POSIX qw/O_CREAT/;
sub test {
   my $val = shift;
   my $res = B::Deparse::Wrapper::getcode($val);
   like( $res, qr/use warnings/);
}
my ($q,$p);
my $x=sub { ++$q,++$p };
test($x);
eval <<EOFCODE and test($x);
   package bar;
   use strict;
   use warnings;
   use warnings::register;
   package main;
   1
EOFCODE

__DATA__
# 2
1;
####
# 3
{
    no warnings;
    '???';
    2;
}
####
# 4
my $test;
++$test and $test /= 2;
>>>>
my $test;
$test /= 2 if ++$test;
####
# 5
-((1, 2) x 2);
####
# 6
{
    my $test = sub : lvalue {
	my $x;
    }
    ;
}
####
# 7
{
    my $test = sub : method {
	my $x;
    }
    ;
}
####
# 8
{
    my $test = sub : locked method {
	my $x;
    }
    ;
}
####
# 9
{
    234;
}
continue {
    123;
}
####
# 10
my $x;
print $main::x;
####
# 11
my @x;
print $main::x[1];
####
# 12
my %x;
$x{warn()};
####
# 13
my $foo;
$_ .= <ARGV> . <$foo>;
####
# 14
my $foo = "Ab\x{100}\200\x{200}\377Cd\000Ef\x{1000}\cA\x{2000}\cZ";
####
# 15
s/x/'y';/e;
####
# 16 - various lypes of loop
{ my $x; }
####
# 17
while (1) { my $k; }
####
# 18
my ($x,@a);
$x=1 for @a;
>>>>
my($x, @a);
$x = 1 foreach (@a);
####
# 19
for (my $i = 0; $i < 2;) {
    my $z = 1;
}
####
# 20
for (my $i = 0; $i < 2; ++$i) {
    my $z = 1;
}
####
# 21
for (my $i = 0; $i < 2; ++$i) {
    my $z = 1;
}
####
# 22
my $i;
while ($i) { my $z = 1; } continue { $i = 99; }
####
# 23
foreach my $i (1, 2) {
    my $z = 1;
}
####
# 24
my $i;
foreach $i (1, 2) {
    my $z = 1;
}
####
# 25
my $i;
foreach my $i (1, 2) {
    my $z = 1;
}
####
# 26
foreach my $i (1, 2) {
    my $z = 1;
}
####
# 27
foreach our $i (1, 2) {
    my $z = 1;
}
####
# 28
my $i;
foreach our $i (1, 2) {
    my $z = 1;
}
####
# 29
my @x;
print reverse sort(@x);
####
# 30
my @x;
print((sort {$b cmp $a} @x));
####
# 31
my @x;
print((reverse sort {$b <=> $a} @x));
####
# 32
our @a;
print $_ foreach (reverse @a);
####
# 33
our @a;
print $_ foreach (reverse 1, 2..5);
####
# 34  (bug #38684)
our @ary;
@ary = split(' ', 'foo', 0);
####
# 35 (bug #40055)
do { () }; 
####
# 36 (ibid.)
do { my $x = 1; $x }; 
####
# 37 <20061012113037.GJ25805@c4.convolution.nl>
my $f = sub {
    +{[]};
} ;
####
# 38 (bug #43010)
'!@$%'->();
####
# 39 (ibid.)
::();
####
# 40 (ibid.)
'::::'->();
####
# 41 (ibid.)
&::::;
####
# 42
my $bar;
'Foo'->$bar('orz');
####
# 43
'Foo'->bar('orz');
####
# 44
'Foo'->bar;
####
# SKIP ?$] < 5.010 && "say not implemented on this Perl version"
# 45 say
say 'foo';
####
# SKIP ?$] < 5.010 && "state vars not implemented on this Perl version"
# 46 state vars
state $x = 42;
####
# SKIP ?$] < 5.010 && "state vars not implemented on this Perl version"
# 47 state var assignment
{
    my $y = (state $x = 42);
}
####
# SKIP ?$] < 5.010 && "state vars not implemented on this Perl version"
# 48 state vars in anoymous subroutines
$a = sub {
    state $x;
    return $x++;
}
;
####
# SKIP ?$] < 5.011 && 'each @array not implemented on this Perl version'
# 49 each @array;
each @ARGV;
each @$a;
####
# SKIP ?$] < 5.011 && 'each @array not implemented on this Perl version'
# 50 keys @array; values @array
keys @$a if keys @ARGV;
values @ARGV if values @$a;
####
# 51 Anonymous arrays and hashes, and references to them
my $a = {};
my $b = \{};
my $c = [];
my $d = \[];
####
# SKIP ?$] < 5.010 && "smartmatch and given/when not implemented on this Perl version"
# 52 implicit smartmatch in given/when
given ('foo') {
    when ('bar') { continue; }
    when ($_ ~~ 'quux') { continue; }
    default { 0; }
}
####
# 53 conditions in elsifs (regression in change #33710 which fixed bug #37302)
if ($a) { x(); }
elsif ($b) { x(); }
elsif ($a and $b) { x(); }
elsif ($a or $b) { x(); }
else { x(); }
####
# 54 interpolation in regexps
my($y, $t);
/x${y}z$t/;
####
# TODO new undocumented cpan-bug #33708
# 55  (cpan-bug #33708)
%{$_ || {}}
####
# TODO hash constants not yet fixed
# 56  (cpan-bug #33708)
use constant H => { "#" => 1 }; H->{"#"}
####
# TODO optimized away 0 not yet fixed
# 57  (cpan-bug #33708)
foreach my $i (@_) { 0 }
####
# 58 placeholder for skipped edbe35ea95
1;
####
# 59 placeholder for skipped edbe35ea95
1;
####
# 60 tests that should be constant folded
x() if 1;
x() if GLIPP;
x() if !GLIPP;
x() if GLIPP && GLIPP;
x() if !GLIPP || GLIPP;
x() if do { GLIPP };
x() if do { no warnings 'void'; 5; GLIPP };
x() if do { !GLIPP };
if (GLIPP) { x() } else { z() }
if (!GLIPP) { x() } else { z() }
if (GLIPP) { x() } elsif (GLIPP) { z() }
if (!GLIPP) { x() } elsif (GLIPP) { z() }
if (GLIPP) { x() } elsif (!GLIPP) { z() }
if (!GLIPP) { x() } elsif (!GLIPP) { z() }
if (!GLIPP) { x() } elsif (!GLIPP) { z() } elsif (GLIPP) { t() }
if (!GLIPP) { x() } elsif (!GLIPP) { z() } elsif (!GLIPP) { t() }
if (!GLIPP) { x() } elsif (!GLIPP) { z() } elsif (!GLIPP) { t() }
>>>>
x();
x();
'???';
x();
x();
x();
x();
do {
    '???'
};
do {
    x()
};
do {
    z()
};
do {
    x()
};
do {
    z()
};
do {
    x()
};
'???';
do {
    t()
};
'???';
!1;
####
# TODO Only strict 'refs' currently supported
# 68 strict
no strict;
$x;
####
# TODO Subsets of warnings could be encoded textually, rather than as bitflips.
no warnings 'deprecated';
my $x;
####
# TODO Better test for CPAN #33708 - the deparsed code has different behaviour
use strict;
no warnings;

foreach (0..3) {
    my $x = 2;
    {
	my $x if 0;
	print ++$x, "\n";
    }
}
