#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

$Ok_Level = 0;
my $test = 1;
sub ok ($;$) {
    my($ok, $name) = @_;

    local $_;

    # You have to do it this way or VMS will get confused.
    printf "%s $test%s\n", $ok   ? 'ok' : 'not ok',
                           $name ? " - $name" : '';

    printf "# Failed test at line %d\n", (caller($Ok_Level))[2] unless $ok;

    $test++;
    return $ok;
}

sub nok ($;$) {
    my($nok, $name) = @_;
    local $Ok_Level = 1;
    ok( !$nok, $name );
}

use Config;
my $have_alarm = $Config{d_alarm};
sub alarm_ok (&) {
    my $test = shift;

    local $SIG{ALRM} = sub { die "timeout\n" };
    
    my $match;
    eval { 
        alarm(2) if $have_alarm;
        $match = $test->();
        alarm(0) if $have_alarm;
    };

    local $Ok_Level = 1;
    ok( !$match && !$@, 'testing studys that used to hang' );
}


print "1..26\n";

$x = "abc\ndef\n";
study($x);

ok($x =~ /^abc/);
ok($x !~ /^def/);

$* = 1;
ok($x =~ /^def/);
$* = 0;

$_ = '123';
study;
ok(/^([0-9][0-9]*)/);

nok($x =~ /^xxx/);
nok($x !~ /^abc/);

ok($x =~ /def/);
nok($x !~ /def/);

study($x);
ok($x !~ /.def/);
nok($x =~ /.def/);

ok($x =~ /\ndef/);
nok($x !~ /\ndef/);

$_ = 'aaabbbccc';
study;
ok(/(a*b*)(c*)/ && $1 eq 'aaabbb' && $2 eq 'ccc');
ok(/(a+b+c+)/ && $1 eq 'aaabbbccc');

nok(/a+b?c+/);

$_ = 'aaabccc';
study;
ok(/a+b?c+/);
ok(/a*b+c*/);

$_ = 'aaaccc';
study;
ok(/a*b?c*/);
nok(/a*b+c*/);

$_ = 'abcdef';
study;
ok(/bcd|xyz/);
ok(/xyz|bcd/);

ok(m|bc/*d|);

ok(/^$_$/);

$* = 1;	    # test 3 only tested the optimized version--this one is for real
ok("ab\ncd\n" =~ /^cd/);

if ($^O eq 'os390' or $^O eq 'posix-bc' or $^O eq 'MacOS') {
    # Even with the alarm() OS/390 and BS2000 can't manage these tests
    # (Perl just goes into a busy loop, luckily an interruptable one)
    for (25..26) { print "not ok $_ # TODO compiler bug?\n" }
    $test += 2;
} else {
    # [ID 20010618.006] tests 25..26 may loop

    $_ = 'FGF';
    study;
    alarm_ok { /G.F$/ };
    alarm_ok { /[F]F$/ };
}

