#!./perl

print "1..78\n";

for ($i = 0; $i <= 10; $i++) {
    $x[$i] = $i;
}
$y = $x[10];
print "#1	:$y: eq :10:\n";
$y = join(' ', @x);
print "#1	:$y: eq :0 1 2 3 4 5 6 7 8 9 10:\n";
if (join(' ', @x) eq '0 1 2 3 4 5 6 7 8 9 10') {
	print "ok 1\n";
} else {
	print "not ok 1\n";
}

$i = $c = 0;
for (;;) {
	$c++;
	last if $i++ > 10;
}
if ($c == 12) {print "ok 2\n";} else {print "not ok 2\n";}

$foo = 3210;
@ary = (1,2,3,4,5);
foreach $foo (@ary) {
	$foo *= 2;
}
if (join('',@ary) eq '246810') {print "ok 3\n";} else {print "not ok 3\n";}

for (@ary) {
    s/(.*)/ok $1\n/;
}

print $ary[1];

# test for internal scratch array generation
# this also tests that $foo was restored to 3210 after test 3
for (split(' ','a b c d e')) {
	$foo .= $_;
}
if ($foo eq '3210abcde') {print "ok 5\n";} else {print "not ok 5 $foo\n";}

foreach $foo (("ok 6\n","ok 7\n")) {
	print $foo;
}

sub foo {
    for $i (1..5) {
	return $i if $_[0] == $i;
    }
}

print foo(1) == 1 ? "ok" : "not ok", " 8\n";
print foo(2) == 2 ? "ok" : "not ok", " 9\n";
print foo(5) == 5 ? "ok" : "not ok", " 10\n";

sub bar {
    return (1, 2, 4);
}

$a = 0;
foreach $b (bar()) {
    $a += $b;
}
print $a == 7 ? "ok" : "not ok", " 11\n";

$loop_count = 0;
for ("-3" .. "0") {
    $loop_count++;
}
print $loop_count == 4 ? "ok" : "not ok", " 12\n";

# modifying arrays in loops is a no-no
@a = (3,4);
eval { @a = () for (1,2,@a) };
print $@ =~ /Use of freed value in iteration/ ? "ok" : "not ok", " 13\n";

# [perl #30061] double destory when same iterator variable (eg $_) used in
# DESTROY as used in for loop that triggered the destroy

{

    my $x = 0;
    sub X::DESTROY {
	my $o = shift;
	$x++;
	1 for (1);
    }

    my %h;
    $h{foo} = bless [], 'X';
    delete $h{foo} for $h{foo}, 1;
    print $x == 1 ? "ok" : "not ok", " 14 - double destroy, x=$x\n";
}

# A lot of tests to check that reversed for works.
my $test = 14;
sub is {
    my ($got, $expected, $name) = @_;
    ++$test;
    if ($got eq $expected) {
	print "ok $test # $name\n";
	return 1;
    }
    print "not ok $test # $name\n";
    print "# got '$got', expected '$expected'\n";
    return 0;
}

@array = ('A', 'B', 'C');
for (@array) {
    $r .= $_;
}
is ($r, 'ABC', 'Forwards for array');
$r = '';
for (1,2,3) {
    $r .= $_;
}
is ($r, '123', 'Forwards for list');
$r = '';
for (map {$_} @array) {
    $r .= $_;
}
is ($r, 'ABC', 'Forwards for array via map');
$r = '';
for (map {$_} 1,2,3) {
    $r .= $_;
}
is ($r, '123', 'Forwards for list via map');

$r = '';
for (reverse @array) {
    $r .= $_;
}
is ($r, 'CBA', 'Reverse for array');
$r = '';
for (reverse 1,2,3) {
    $r .= $_;
}
is ($r, '321', 'Reverse for list');
$r = '';
for (reverse map {$_} @array) {
    $r .= $_;
}
is ($r, 'CBA', 'Reverse for array via map');
$r = '';
for (reverse map {$_} 1,2,3) {
    $r .= $_;
}
is ($r, '321', 'Reverse for list via map');

$r = '';
for my $i (@array) {
    $r .= $i;
}
is ($r, 'ABC', 'Forwards for array with var');
$r = '';
for my $i (1,2,3) {
    $r .= $i;
}
is ($r, '123', 'Forwards for list with var');
$r = '';
for my $i (map {$_} @array) {
    $r .= $i;
}
is ($r, 'ABC', 'Forwards for array via map with var');
$r = '';
for my $i (map {$_} 1,2,3) {
    $r .= $i;
}
is ($r, '123', 'Forwards for list via map with var');

$r = '';
for my $i (reverse @array) {
    $r .= $i;
}
is ($r, 'CBA', 'Reverse for array with var');
$r = '';
for my $i (reverse 1,2,3) {
    $r .= $i;
}
is ($r, '321', 'Reverse for list with var');
$r = '';
for my $i (reverse map {$_} @array) {
    $r .= $i;
}
is ($r, 'CBA', 'Reverse for array via map with var');
$r = '';
for my $i (reverse map {$_} 1,2,3) {
    $r .= $i;
}
is ($r, '321', 'Reverse for list via map with var');

# For some reason the generate optree is different when $_ is implicit.
$r = '';
for $_ (@array) {
    $r .= $_;
}
is ($r, 'ABC', 'Forwards for array with explicit $_');
$r = '';
for $_ (1,2,3) {
    $r .= $_;
}
is ($r, '123', 'Forwards for list with explicit $_');
$r = '';
for $_ (map {$_} @array) {
    $r .= $_;
}
is ($r, 'ABC', 'Forwards for array via map with explicit $_');
$r = '';
for $_ (map {$_} 1,2,3) {
    $r .= $_;
}
is ($r, '123', 'Forwards for list via map with explicit $_');

$r = '';
for $_ (reverse @array) {
    $r .= $_;
}
is ($r, 'CBA', 'Reverse for array with explicit $_');
$r = '';
for $_ (reverse 1,2,3) {
    $r .= $_;
}
is ($r, '321', 'Reverse for list with explicit $_');
$r = '';
for $_ (reverse map {$_} @array) {
    $r .= $_;
}
is ($r, 'CBA', 'Reverse for array via map with explicit $_');
$r = '';
for $_ (reverse map {$_} 1,2,3) {
    $r .= $_;
}
is ($r, '321', 'Reverse for list via map with explicit $_');

# I don't think that my is that different from our in the optree. But test a
# few:
$r = '';
for our $i (reverse @array) {
    $r .= $i;
}
is ($r, 'CBA', 'Reverse for array with our var');
$r = '';
for our $i (reverse 1,2,3) {
    $r .= $i;
}
is ($r, '321', 'Reverse for list with our var');
$r = '';
for our $i (reverse map {$_} @array) {
    $r .= $i;
}
is ($r, 'CBA', 'Reverse for array via map with our var');
$r = '';
for our $i (reverse map {$_} 1,2,3) {
    $r .= $i;
}
is ($r, '321', 'Reverse for list via map with our var');


$r = '';
for (1, reverse @array) {
    $r .= $_;
}
is ($r, '1CBA', 'Reverse for array with leading value');
$r = '';
for ('A', reverse 1,2,3) {
    $r .= $_;
}
is ($r, 'A321', 'Reverse for list with leading value');
$r = '';
for (1, reverse map {$_} @array) {
    $r .= $_;
}
is ($r, '1CBA', 'Reverse for array via map with leading value');
$r = '';
for ('A', reverse map {$_} 1,2,3) {
    $r .= $_;
}
is ($r, 'A321', 'Reverse for list via map with leading value');

$r = '';
for (reverse (@array), 1) {
    $r .= $_;
}
is ($r, 'CBA1', 'Reverse for array with trailing value');
$r = '';
for (reverse (1,2,3), 'A') {
    $r .= $_;
}
is ($r, '321A', 'Reverse for list with trailing value');
$r = '';
for (reverse (map {$_} @array), 1) {
    $r .= $_;
}
is ($r, 'CBA1', 'Reverse for array via map with trailing value');
$r = '';
for (reverse (map {$_} 1,2,3), 'A') {
    $r .= $_;
}
is ($r, '321A', 'Reverse for list via map with trailing value');


$r = '';
for $_ (1, reverse @array) {
    $r .= $_;
}
is ($r, '1CBA', 'Reverse for array with leading value with explicit $_');
$r = '';
for $_ ('A', reverse 1,2,3) {
    $r .= $_;
}
is ($r, 'A321', 'Reverse for list with leading value with explicit $_');
$r = '';
for $_ (1, reverse map {$_} @array) {
    $r .= $_;
}
is ($r, '1CBA',
    'Reverse for array via map with leading value with explicit $_');
$r = '';
for $_ ('A', reverse map {$_} 1,2,3) {
    $r .= $_;
}
is ($r, 'A321', 'Reverse for list via map with leading value with explicit $_');

$r = '';
for $_ (reverse (@array), 1) {
    $r .= $_;
}
is ($r, 'CBA1', 'Reverse for array with trailing value with explicit $_');
$r = '';
for $_ (reverse (1,2,3), 'A') {
    $r .= $_;
}
is ($r, '321A', 'Reverse for list with trailing value with explicit $_');
$r = '';
for $_ (reverse (map {$_} @array), 1) {
    $r .= $_;
}
is ($r, 'CBA1',
    'Reverse for array via map with trailing value with explicit $_');
$r = '';
for $_ (reverse (map {$_} 1,2,3), 'A') {
    $r .= $_;
}
is ($r, '321A',
    'Reverse for list via map with trailing value with explicit $_');

$r = '';
for my $i (1, reverse @array) {
    $r .= $i;
}
is ($r, '1CBA', 'Reverse for array with leading value and var');
$r = '';
for my $i ('A', reverse 1,2,3) {
    $r .= $i;
}
is ($r, 'A321', 'Reverse for list with leading value and var');
$r = '';
for my $i (1, reverse map {$_} @array) {
    $r .= $i;
}
is ($r, '1CBA', 'Reverse for array via map with leading value and var');
$r = '';
for my $i ('A', reverse map {$_} 1,2,3) {
    $r .= $i;
}
is ($r, 'A321', 'Reverse for list via map with leading value and var');

$r = '';
for my $i (reverse (@array), 1) {
    $r .= $i;
}
is ($r, 'CBA1', 'Reverse for array with trailing value and var');
$r = '';
for my $i (reverse (1,2,3), 'A') {
    $r .= $i;
}
is ($r, '321A', 'Reverse for list with trailing value and var');
$r = '';
for my $i (reverse (map {$_} @array), 1) {
    $r .= $i;
}
is ($r, 'CBA1', 'Reverse for array via map with trailing value and var');
$r = '';
for my $i (reverse (map {$_} 1,2,3), 'A') {
    $r .= $i;
}
is ($r, '321A', 'Reverse for list via map with trailing value and var');


$r = '';
for (reverse 1, @array) {
    $r .= $_;
}
is ($r, 'CBA1', 'Reverse for value and array');
$r = '';
for (reverse map {$_} 1, @array) {
    $r .= $_;
}
is ($r, 'CBA1', 'Reverse for value and array via map');

$r = '';
for (reverse (@array, 1)) {
    $r .= $_;
}
is ($r, '1CBA', 'Reverse for array and value');
$r = '';
for (reverse (map {$_} @array, 1)) {
    $r .= $_;
}
is ($r, '1CBA', 'Reverse for array and value via map');

$r = '';
for $_ (reverse 1, @array) {
    $r .= $_;
}
is ($r, 'CBA1', 'Reverse for value and array with explicit $_');
$r = '';
for $_ (reverse map {$_} 1, @array) {
    $r .= $_;
}
is ($r, 'CBA1', 'Reverse for value and array via map with explicit $_');

$r = '';
for $_ (reverse (@array, 1)) {
    $r .= $_;
}
is ($r, '1CBA', 'Reverse for array and value with explicit $_');
$r = '';
for $_ (reverse (map {$_} @array, 1)) {
    $r .= $_;
}
is ($r, '1CBA', 'Reverse for array and value via map with explicit $_');


$r = '';
for my $i (reverse 1, @array) {
    $r .= $i;
}
is ($r, 'CBA1', 'Reverse for value and array with var');
$r = '';
for my $i (reverse map {$_} 1, @array) {
    $r .= $i;
}
is ($r, 'CBA1', 'Reverse for value and array via map with var');

$r = '';
for my $i (reverse (@array, 1)) {
    $r .= $i;
}
is ($r, '1CBA', 'Reverse for array and value with var');
$r = '';
for my $i (reverse (map {$_} @array, 1)) {
    $r .= $i;
}
is ($r, '1CBA', 'Reverse for array and value via map with var');
