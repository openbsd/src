#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 16;

# compile time evaluation

$s = sqrt(2);
is(substr($s,0,5), '1.414', 'compile time sqrt(2) == 1.414');

$s = exp(1);
is(substr($s,0,7), '2.71828', 'compile time exp(1) == e');

cmp_ok(exp(log(1)), '==', 1, 'compile time exp(log(1)) == 1');

# run time evaluation

$x1 = 1;
$x2 = 2;
$s = sqrt($x2);
is(substr($s,0,5), '1.414', 'run time sqrt(2) == 1.414');

$s = exp($x1);
is(substr($s,0,7), '2.71828', 'run time exp(1) = e');

cmp_ok(exp(log($x1)), '==', 1, 'run time exp(log(1)) == 1');

# tests for transcendental functions

my $pi = 3.1415926535897931160;
my $pi_2 = 1.5707963267948965580;

sub round {
   my $result = shift;
   return sprintf("%.9f", $result);
}

# sin() tests
cmp_ok(sin(0), '==', 0.0, 'sin(0) == 0');
cmp_ok(round(sin($pi)), '==', 0.0, 'sin(pi) == 0');
cmp_ok(round(sin(-1 * $pi)), '==', 0.0, 'sin(-pi) == 0');
cmp_ok(round(sin($pi_2)), '==', 1.0, 'sin(pi/2) == 1');
cmp_ok(round(sin(-1 * $pi_2)), '==', -1.0, 'sin(-pi/2) == -1');

# cos() tests
cmp_ok(cos(0), '==', 1.0, 'cos(0) == 1');
cmp_ok(round(cos($pi)), '==', -1.0, 'cos(pi) == -1');
cmp_ok(round(cos(-1 * $pi)), '==', -1.0, 'cos(-pi) == -1');
cmp_ok(round(cos($pi_2)), '==', 0.0, 'cos(pi/2) == 0');
cmp_ok(round(cos(-1 * $pi_2)), '==', 0.0, 'cos(-pi/2) == 0');

# atan2() tests were removed due to differing results from calls to
# atan2() on various OS's and architectures.  See perlport.pod for
# more information.
