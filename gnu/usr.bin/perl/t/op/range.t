#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = ('../lib', '.');
}   
# Avoid using eq_array below as it uses .. internally.
require './test.pl';

use Config;

plan (141);

is(join(':',1..5), '1:2:3:4:5');

@foo = (1,2,3,4,5,6,7,8,9);
@foo[2..4] = ('c','d','e');

is(join(':',@foo[$foo[0]..5]), '2:c:d:e:6');

@bar[2..4] = ('c','d','e');
is(join(':',@bar[1..5]), ':c:d:e:');

($a,@bcd[0..2],$e) = ('a','b','c','d','e');
is(join(':',$a,@bcd[0..2],$e), 'a:b:c:d:e');

$x = 0;
for (1..100) {
    $x += $_;
}
is($x, 5050);

$x = 0;
for ((100,2..99,1)) {
    $x += $_;
}
is($x, 5050);

$x = join('','a'..'z');
is($x, 'abcdefghijklmnopqrstuvwxyz');

@x = 'A'..'ZZ';
is (scalar @x, 27 * 26);

@x = '09' .. '08';  # should produce '09', '10',... '99' (strange but true)
is(join(",", @x), join(",", map {sprintf "%02d",$_} 9..99));

# same test with foreach (which is a separate implementation)
@y = ();
foreach ('09'..'08') {
    push(@y, $_);
}
is(join(",", @y), join(",", @x));

# check bounds
if ($Config{ivsize} == 8) {
  @a = eval "0x7ffffffffffffffe..0x7fffffffffffffff";
  $a = "9223372036854775806 9223372036854775807";
  @b = eval "-0x7fffffffffffffff..-0x7ffffffffffffffe";
  $b = "-9223372036854775807 -9223372036854775806";
}
else {
  @a = eval "0x7ffffffe..0x7fffffff";
  $a = "2147483646 2147483647";
  @b = eval "-0x7fffffff..-0x7ffffffe";
  $b = "-2147483647 -2147483646";
}

is ("@a", $a);

is ("@b", $b);

# check magic
{
    my $bad = 0;
    local $SIG{'__WARN__'} = sub { $bad = 1 };
    my $x = 'a-e';
    $x =~ s/(\w)-(\w)/join ':', $1 .. $2/e;
    is ($x, 'a:b:c:d:e');
}

# Should use magical autoinc only when both are strings
{
    my $scalar = (() = "0"..-1);
    is ($scalar, 0);
}
{
    my $fail = 0;
    for my $x ("0"..-1) {
	$fail++;
    }
    is ($fail, 0);
}

# [#18165] Should allow "-4".."0", broken by #4730. (AMS 20021031)
is(join(":","-4".."0")     , "-4:-3:-2:-1:0");
is(join(":","-4".."-0")    , "-4:-3:-2:-1:0");
is(join(":","-4\n".."0\n") , "-4:-3:-2:-1:0");
is(join(":","-4\n".."-0\n"), "-4:-3:-2:-1:0");

# undef should be treated as 0 for numerical range
is(join(":",undef..2), '0:1:2');
is(join(":",-2..undef), '-2:-1:0');
is(join(":",undef..'2'), '0:1:2');
is(join(":",'-2'..undef), '-2:-1:0');

# undef should be treated as "" for magical range
is(join(":", map "[$_]", "".."B"), '[]');
is(join(":", map "[$_]", undef.."B"), '[]');
is(join(":", map "[$_]", "B"..""), '');
is(join(":", map "[$_]", "B"..undef), '');

# undef..undef used to segfault
is(join(":", map "[$_]", undef..undef), '[]');

# also test undef in foreach loops
@foo=(); push @foo, $_ for undef..2;
is(join(":", @foo), '0:1:2');

@foo=(); push @foo, $_ for -2..undef;
is(join(":", @foo), '-2:-1:0');

@foo=(); push @foo, $_ for undef..'2';
is(join(":", @foo), '0:1:2');

@foo=(); push @foo, $_ for '-2'..undef;
is(join(":", @foo), '-2:-1:0');

@foo=(); push @foo, $_ for undef.."B";
is(join(":", map "[$_]", @foo), '[]');

@foo=(); push @foo, $_ for "".."B";
is(join(":", map "[$_]", @foo), '[]');

@foo=(); push @foo, $_ for "B"..undef;
is(join(":", map "[$_]", @foo), '');

@foo=(); push @foo, $_ for "B".."";
is(join(":", map "[$_]", @foo), '');

@foo=(); push @foo, $_ for undef..undef;
is(join(":", map "[$_]", @foo), '[]');

# again with magic
{
    my @a = (1..3);
    @foo=(); push @foo, $_ for undef..$#a;
    is(join(":", @foo), '0:1:2');
}
{
    my @a = ();
    @foo=(); push @foo, $_ for $#a..undef;
    is(join(":", @foo), '-1:0');
}
{
    local $1;
    "2" =~ /(.+)/;
    @foo=(); push @foo, $_ for undef..$1;
    is(join(":", @foo), '0:1:2');
}
{
    local $1;
    "-2" =~ /(.+)/;
    @foo=(); push @foo, $_ for $1..undef;
    is(join(":", @foo), '-2:-1:0');
}
{
    local $1;
    "B" =~ /(.+)/;
    @foo=(); push @foo, $_ for undef..$1;
    is(join(":", map "[$_]", @foo), '[]');
}
{
    local $1;
    "B" =~ /(.+)/;
    @foo=(); push @foo, $_ for ""..$1;
    is(join(":", map "[$_]", @foo), '[]');
}
{
    local $1;
    "B" =~ /(.+)/;
    @foo=(); push @foo, $_ for $1..undef;
    is(join(":", map "[$_]", @foo), '');
}
{
    local $1;
    "B" =~ /(.+)/;
    @foo=(); push @foo, $_ for $1.."";
    is(join(":", map "[$_]", @foo), '');
}

# Test upper range limit
my $MAX_INT = ~0>>1;

foreach my $ii (-3 .. 3) {
    my ($first, $last);
    eval {
        my $lim=0;
        for ($MAX_INT-10 .. $MAX_INT+$ii) {
            if (! defined($first)) {
                $first = $_;
            }
            $last = $_;
            last if ($lim++ > 100);   # Protect against integer wrap
        }
    };
    if ($ii <= 0) {
        ok(! $@, 'Upper bound accepted: ' . ($MAX_INT+$ii));
        is($first, $MAX_INT-10, 'Lower bound okay');
        is($last, $MAX_INT+$ii, 'Upper bound okay');
    } else {
        ok($@, 'Upper bound rejected: ' . ($MAX_INT+$ii));
    }
}

foreach my $ii (-3 .. 3) {
    my ($first, $last);
    eval {
        my $lim=0;
        for ($MAX_INT+$ii .. $MAX_INT) {
            if (! defined($first)) {
                $first = $_;
            }
            $last = $_;
            last if ($lim++ > 100);
        }
    };
    if ($ii <= 0) {
        ok(! $@, 'Lower bound accepted: ' . ($MAX_INT+$ii));
        is($first, $MAX_INT+$ii, 'Lower bound okay');
        is($last, $MAX_INT, 'Upper bound okay');
    } else {
        ok($@, 'Lower bound rejected: ' . ($MAX_INT+$ii));
    }
}

{
    my $first;
    eval {
        my $lim=0;
        for ($MAX_INT .. $MAX_INT-1) {
            if (! defined($first)) {
                $first = $_;
            }
            $last = $_;
            last if ($lim++ > 100);
        }
    };
    ok(! $@, 'Range accepted');
    ok(! defined($first), 'Range ineffectual');
}

foreach my $ii (~0, ~0+1, ~0+(~0>>4)) {
    eval {
        my $lim=0;
        for ($MAX_INT-10 .. $ii) {
            last if ($lim++ > 100);
        }
    };
    ok($@, 'Upper bound rejected: ' . $ii);
}

# Test lower range limit
my $MIN_INT = -1-$MAX_INT;

if (! $Config{d_nv_preserves_uv}) {
    # $MIN_INT needs adjustment when IV won't fit into an NV
    my $NV = $MIN_INT - 1;
    my $OFFSET = 1;
    while (($NV + $OFFSET) == $MIN_INT) {
        $OFFSET++
    }
    $MIN_INT += $OFFSET;
}

foreach my $ii (-3 .. 3) {
    my ($first, $last);
    eval {
        my $lim=0;
        for ($MIN_INT+$ii .. $MIN_INT+10) {
            if (! defined($first)) {
                $first = $_;
            }
            $last = $_;
            last if ($lim++ > 100);
        }
    };
    if ($ii >= 0) {
        ok(! $@, 'Lower bound accepted: ' . ($MIN_INT+$ii));
        is($first, $MIN_INT+$ii, 'Lower bound okay');
        is($last, $MIN_INT+10, 'Upper bound okay');
    } else {
        ok($@, 'Lower bound rejected: ' . ($MIN_INT+$ii));
    }
}

foreach my $ii (-3 .. 3) {
    my ($first, $last);
    eval {
        my $lim=0;
        for ($MIN_INT .. $MIN_INT+$ii) {
            if (! defined($first)) {
                $first = $_;
            }
            $last = $_;
            last if ($lim++ > 100);
        }
    };
    if ($ii >= 0) {
        ok(! $@, 'Upper bound accepted: ' . ($MIN_INT+$ii));
        is($first, $MIN_INT, 'Lower bound okay');
        is($last, $MIN_INT+$ii, 'Upper bound okay');
    } else {
        ok($@, 'Upper bound rejected: ' . ($MIN_INT+$ii));
    }
}

{
    my $first;
    eval {
        my $lim=0;
        for ($MIN_INT+1 .. $MIN_INT) {
            if (! defined($first)) {
                $first = $_;
            }
            $last = $_;
            last if ($lim++ > 100);
        }
    };
    ok(! $@, 'Range accepted');
    ok(! defined($first), 'Range ineffectual');
}

foreach my $ii (~0, ~0+1, ~0+(~0>>4)) {
    eval {
        my $lim=0;
        for (-$ii .. $MIN_INT+10) {
            last if ($lim++ > 100);
        }
    };
    ok($@, 'Lower bound rejected: ' . -$ii);
}

# double/triple magic tests
sub TIESCALAR { bless { value => $_[1], orig => $_[1] } }
sub STORE { $_[0]{store}++; $_[0]{value} = $_[1] }
sub FETCH { $_[0]{fetch}++; $_[0]{value} }
sub stores { tied($_[0])->{value} = tied($_[0])->{orig};
             delete(tied($_[0])->{store}) || 0 }
sub fetches { delete(tied($_[0])->{fetch}) || 0 }
    
tie $x, "main", 6;

my @foo;
@foo = 4 .. $x;
is(scalar @foo, 3);
is("@foo", "4 5 6");
is(fetches($x), 1);
is(stores($x), 0);

@foo = $x .. 8;
is(scalar @foo, 3);
is("@foo", "6 7 8");
is(fetches($x), 1);
is(stores($x), 0);

@foo = $x .. $x + 1;
is(scalar @foo, 2);
is("@foo", "6 7");
is(fetches($x), 2);
is(stores($x), 0);

@foo = ();
for (4 .. $x) {
  push @foo, $_;
}
is(scalar @foo, 3);
is("@foo", "4 5 6");
is(fetches($x), 1);
is(stores($x), 0);

@foo = ();
for (reverse 4 .. $x) {
  push @foo, $_;
}
is(scalar @foo, 3);
is("@foo", "6 5 4");
is(fetches($x), 1);
is(stores($x), 0);

is( ( join ' ', map { join '', map ++$_, ($x=1)..4 } 1..2 ), '2345 2345',
    'modifiable variable num range' );
is( ( join ' ', map { join '', map ++$_, 1..4      } 1..2 ), '2345 2345',
    'modifiable const num range' );  # RT#3105
$s = ''; for (1..2) { for (1..4) { $s .= ++$_ } $s.=' ' if $_==1; }
is( $s, '2345 2345','modifiable num counting loop counter' );


is( ( join ' ', map { join '', map ++$_, ($x='a')..'d' } 1..2 ), 'bcde bcde',
    'modifiable variable alpha range' );
is( ( join ' ', map { join '', map ++$_, 'a'..'d'      } 1..2 ), 'bcde bcde',
    'modifiable const alpha range' );  # RT#3105
$s = ''; for (1..2) { for ('a'..'d') { $s .= ++$_ } $s.=' ' if $_==1; }
is( $s, 'bcde bcde','modifiable alpha counting loop counter' );

# EOF
