#!./perl

sub foo1
{
    ok($_[0]);
    'value';
}

sub foo2
{
    shift;
    ok($_[0]);
    $x = 'value';
    $x;
}

my $test = 1;
sub ok {
    my($ok, $name) = @_;

    # You have to do it this way or VMS will get confused.
    printf "%s %d%s\n", $ok ? "ok" : "not ok", 
                        $test,
                        defined $name ? " - $name" : '';

    printf "# Failed test at line %d\n", (caller)[2] unless $ok;

    $test++;
    return $ok;
}

print "1..50\n";

# Test do &sub and proper @_ handling.
$_[0] = 0;
{
    no warnings 'deprecated';
    $result = do foo1(1);
}

ok( $result eq 'value',  ":$result: eq :value:" );
ok( $_[0] == 0 );

$_[0] = 0;
{
    no warnings 'deprecated';
    $result = do foo2(0,1,0);
}
ok( $result eq 'value', ":$result: eq :value:" );
ok( $_[0] == 0 );

$result = do{ ok 1; 'value';};
ok( $result eq 'value',  ":$result: eq :value:" );

sub blather {
    ok 1 foreach @_;
}

{
    no warnings 'deprecated';
    do blather("ayep","sho nuff");
}
@x = ("jeepers", "okydoke");
@y = ("uhhuh", "yeppers");
{
    no warnings 'deprecated';
    do blather(@x,"noofie",@y);
}

unshift @INC, '.';

if (open(DO, ">$$.16")) {
    print DO "ok(1, 'do in scalar context') if defined wantarray && not wantarray\n";
    close DO or die "Could not close: $!";
}

my $a = do "$$.16"; die $@ if $@;

if (open(DO, ">$$.17")) {
    print DO "ok(1, 'do in list context') if defined wantarray &&     wantarray\n";
    close DO or die "Could not close: $!";
}

my @a = do "$$.17"; die $@ if $@;

if (open(DO, ">$$.18")) {
    print DO "ok(1, 'do in void context') if not defined wantarray\n";
    close DO or die "Could not close: $!";
}

do "$$.18"; die $@ if $@;

# bug ID 20010920.007
eval qq{ do qq(a file that does not exist); };
ok( !$@, "do on a non-existing file, first try" );

eval qq{ do uc qq(a file that does not exist); };
ok( !$@, "do on a non-existing file, second try"  );

# 6 must be interpreted as a file name here
ok( (!defined do 6) && $!, "'do 6' : $!" );

# [perl #19545]
push @t, ($u = (do {} . "This should be pushed."));
ok( $#t == 0, "empty do result value" );

$zok = '';
$owww = do { 1 if $zok };
ok( $owww eq '', 'last is unless' );
$owww = do { 2 unless not $zok };
ok( $owww == 1, 'last is if not' );

$zok = 'swish';
$owww = do { 3 unless $zok };
ok( $owww eq 'swish', 'last is unless' );
$owww = do { 4 if not $zok };
ok( $owww eq '', 'last is if not' );

# [perl #38809]
@a = (7);
$x = sub { do { return do { @a } }; 2 }->();
ok(defined $x && $x == 1, 'return do { } receives caller scalar context');
@x = sub { do { return do { @a } }; 2 }->();
ok("@x" eq "7", 'return do { } receives caller list context');

@a = (7, 8);
$x = sub { do { return do { 1; @a } }; 3 }->();
ok(defined $x && $x == 2, 'return do { ; } receives caller scalar context');
@x = sub { do { return do { 1; @a } }; 3 }->();
ok("@x" eq "7 8", 'return do { ; } receives caller list context');

@b = (11 .. 15);
$x = sub { do { return do { 1; @a, @b } }; 3 }->();
ok(defined $x && $x == 5, 'return do { ; , } receives caller scalar context');
@x = sub { do { return do { 1; @a, @b } }; 3 }->();
ok("@x" eq "7 8 11 12 13 14 15", 'return do { ; , } receives caller list context');

$x = sub { do { return do { 1; @a }, do { 2; @b } }; 3 }->();
ok(defined $x && $x == 5, 'return do { ; }, do { ; } receives caller scalar context');
@x = sub { do { return do { 1; @a }, do { 2; @b } }; 3 }->();
ok("@x" eq "7 8 11 12 13 14 15", 'return do { ; }, do { ; } receives caller list context');

@a = (7, 8, 9);
$x = sub { do { do { 1; return @a } }; 4 }->();
ok(defined $x && $x == 3, 'do { return } receives caller scalar context');
@x = sub { do { do { 1; return @a } }; 4 }->();
ok("@x" eq "7 8 9", 'do { return } receives caller list context');

@a = (7, 8, 9, 10);
$x = sub { do { return do { 1; do { 2; @a } } }; 5 }->();
ok(defined $x && $x == 4, 'return do { do { ; } } receives caller scalar context');
@x = sub { do { return do { 1; do { 2; @a } } }; 5 }->();
ok("@x" eq "7 8 9 10", 'return do { do { ; } } receives caller list context');

# Do blocks created by constant folding
# [perl #68108]
$x = sub { if (1) { 20 } }->();
ok($x == 20, 'if (1) { $x } receives caller scalar context');

@a = (21 .. 23);
$x = sub { if (1) { @a } }->();
ok($x == 3, 'if (1) { @a } receives caller scalar context');
@x = sub { if (1) { @a } }->();
ok("@x" eq "21 22 23", 'if (1) { @a } receives caller list context');

$x = sub { if (1) { 0; 20 } }->();
ok($x == 20, 'if (1) { ...; $x } receives caller scalar context');

@a = (24 .. 27);
$x = sub { if (1) { 0; @a } }->();
ok($x == 4, 'if (1) { ...; @a } receives caller scalar context');
@x = sub { if (1) { 0; @a } }->();
ok("@x" eq "24 25 26 27", 'if (1) { ...; @a } receives caller list context');

$x = sub { if (1) { 0; 20 } else{} }->();
ok($x == 20, 'if (1) { ...; $x } else{} receives caller scalar context');

@a = (24 .. 27);
$x = sub { if (1) { 0; @a } else{} }->();
ok($x == 4, 'if (1) { ...; @a } else{} receives caller scalar context');
@x = sub { if (1) { 0; @a } else{} }->();
ok("@x" eq "24 25 26 27", 'if (1) { ...; @a } else{} receives caller list context');

$x = sub { if (0){} else { 0; 20 } }->();
ok($x == 20, 'if (0){} else { ...; $x } receives caller scalar context');

@a = (24 .. 27);
$x = sub { if (0){} else { 0; @a } }->();
ok($x == 4, 'if (0){} else { ...; @a } receives caller scalar context');
@x = sub { if (0){} else { 0; @a } }->();
ok("@x" eq "24 25 26 27", 'if (0){} else { ...; @a } receives caller list context');


END {
    1 while unlink("$$.16", "$$.17", "$$.18");
}
