#!./perl

BEGIN {
    require "test.pl";
}

plan(106);

# A lot of tests to check that reversed for works.

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
for (1 .. 3) {
    $r .= $_;
}
is ($r, '123', 'Forwards for list via ..');
$r = '';
for ('A' .. 'C') {
    $r .= $_;
}
is ($r, 'ABC', 'Forwards for list via ..');

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
for (reverse 1 .. 3) {
    $r .= $_;
}
is ($r, '321', 'Reverse for list via ..');
$r = '';
for (reverse 'A' .. 'C') {
    $r .= $_;
}
is ($r, 'CBA', 'Reverse for list via ..');

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
for my $i (1 .. 3) {
    $r .= $i;
}
is ($r, '123', 'Forwards for list via .. with var');
$r = '';
for my $i ('A' .. 'C') {
    $r .= $i;
}
is ($r, 'ABC', 'Forwards for list via .. with var');

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
$r = '';
for my $i (reverse 1 .. 3) {
    $r .= $i;
}
is ($r, '321', 'Reverse for list via .. with var');
$r = '';
for my $i (reverse 'A' .. 'C') {
    $r .= $i;
}
is ($r, 'CBA', 'Reverse for list via .. with var');

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
for $_ (1 .. 3) {
    $r .= $_;
}
is ($r, '123', 'Forwards for list via .. with var with explicit $_');
$r = '';
for $_ ('A' .. 'C') {
    $r .= $_;
}
is ($r, 'ABC', 'Forwards for list via .. with var with explicit $_');

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
$r = '';
for $_ (reverse 1 .. 3) {
    $r .= $_;
}
is ($r, '321', 'Reverse for list via .. with var with explicit $_');
$r = '';
for $_ (reverse 'A' .. 'C') {
    $r .= $_;
}
is ($r, 'CBA', 'Reverse for list via .. with var with explicit $_');

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
for our $i (reverse 1 .. 3) {
    $r .= $i;
}
is ($r, '321', 'Reverse for list via .. with our var');
$r = '';
for our $i (reverse 'A' .. 'C') {
    $r .= $i;
}
is ($r, 'CBA', 'Reverse for list via .. with our var');


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
for ('A', reverse 1 .. 3) {
    $r .= $_;
}
is ($r, 'A321', 'Reverse for list via .. with leading value');
$r = '';
for (1, reverse 'A' .. 'C') {
    $r .= $_;
}
is ($r, '1CBA', 'Reverse for list via .. with leading value');

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
for (reverse (1 .. 3), 'A') {
    $r .= $_;
}
is ($r, '321A', 'Reverse for list via .. with trailing value');
$r = '';
for (reverse ('A' .. 'C'), 1) {
    $r .= $_;
}
is ($r, 'CBA1', 'Reverse for list via .. with trailing value');


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
for $_ ('A', reverse 1 .. 3) {
    $r .= $_;
}
is ($r, 'A321', 'Reverse for list via .. with leading value with explicit $_');
$r = '';
for $_ (1, reverse 'A' .. 'C') {
    $r .= $_;
}
is ($r, '1CBA', 'Reverse for list via .. with leading value with explicit $_');

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
for $_ (reverse (1 .. 3), 'A') {
    $r .= $_;
}
is ($r, '321A', 'Reverse for list via .. with trailing value with explicit $_');
$r = '';
for $_ (reverse ('A' .. 'C'), 1) {
    $r .= $_;
}
is ($r, 'CBA1', 'Reverse for list via .. with trailing value with explicit $_');

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
for my $i ('A', reverse 1 .. 3) {
    $r .= $i;
}
is ($r, 'A321', 'Reverse for list via .. with leading value and var');
$r = '';
for my $i (1, reverse 'A' .. 'C') {
    $r .= $i;
}
is ($r, '1CBA', 'Reverse for list via .. with leading value and var');

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
for my $i (reverse (1 .. 3), 'A') {
    $r .= $i;
}
is ($r, '321A', 'Reverse for list via .. with trailing value and var');
$r = '';
for my $i (reverse ('A' .. 'C'), 1) {
    $r .= $i;
}
is ($r, 'CBA1', 'Reverse for list via .. with trailing value and var');


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
for (reverse 1 .. 3, @array) {
    $r .= $_;
}
is ($r, 'CBA321', 'Reverse for .. and array');
$r = '';
for (reverse 'X' .. 'Z', @array) {
    $r .= $_;
}
is ($r, 'CBAZYX', 'Reverse for .. and array');
$r = '';
for (reverse map {$_} 1 .. 3, @array) {
    $r .= $_;
}
is ($r, 'CBA321', 'Reverse for .. and array via map');
$r = '';
for (reverse map {$_} 'X' .. 'Z', @array) {
    $r .= $_;
}
is ($r, 'CBAZYX', 'Reverse for .. and array via map');

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
for $_ (reverse 1 .. 3, @array) {
    $r .= $_;
}
is ($r, 'CBA321', 'Reverse for .. and array with explicit $_');
$r = '';
for $_ (reverse 'X' .. 'Z', @array) {
    $r .= $_;
}
is ($r, 'CBAZYX', 'Reverse for .. and array with explicit $_');
$r = '';
for $_ (reverse map {$_} 1 .. 3, @array) {
    $r .= $_;
}
is ($r, 'CBA321', 'Reverse for .. and array via map with explicit $_');
$r = '';
for $_ (reverse map {$_} 'X' .. 'Z', @array) {
    $r .= $_;
}
is ($r, 'CBAZYX', 'Reverse for .. and array via map with explicit $_');

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
for my $i (reverse 1 .. 3, @array) {
    $r .= $i;
}
is ($r, 'CBA321', 'Reverse for .. and array with var');
$r = '';
for my $i (reverse 'X' .. 'Z', @array) {
    $r .= $i;
}
is ($r, 'CBAZYX', 'Reverse for .. and array with var');
$r = '';
for my $i (reverse map {$_} 1 .. 3, @array) {
    $r .= $i;
}
is ($r, 'CBA321', 'Reverse for .. and array via map with var');
$r = '';
for my $i (reverse map {$_} 'X' .. 'Z', @array) {
    $r .= $i;
}
is ($r, 'CBAZYX', 'Reverse for .. and array via map with var');

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

TODO: {
    if (do {17; foreach (1, 2) { 1; } } != 17) {
        #print "not ";
	todo_skip("RT #1085: what should be output of perl -we 'print do { foreach (1, 2) { 1; } }'");
     }
}

TODO: {
    local $TODO = "RT #2166: foreach spuriously autovivifies";
    my %h;
    foreach (@h{a, b}) {}
    if(keys(%h)) {
        todo_skip("RT #2166: foreach spuriously autovivifies");
    }
}

sub {
    foreach (@_) {
        is eval { \$_ }, \undef, 'foreach (@array_containing_undef)'
    }
}->(undef);

SKIP: {
    skip "No XS::APItest under miniperl", 1, if is_miniperl;
    skip "no XS::APItest", 1 if !eval { require XS::APItest };
    my @a;
    sub {
        XS::APItest::alias_av(\@a, 0, undef);
        eval { \$_[0] }
    }->($a[0]);
    is $@, "", 'vivify_defelem does not croak on &PL_sv_undef elements';
}
