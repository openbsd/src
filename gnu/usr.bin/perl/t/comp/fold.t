#!./perl

BEGIN {
    chdir 't';
    @INC = '../lib';
    require './test.pl';
}
use strict;
use warnings;

plan (13);

# Historically constant folding was performed by evaluating the ops, and if
# they threw an exception compilation failed. This was seen as buggy, because
# even illegal constants in unreachable code would cause failure. So now
# illegal expressions are reported at runtime, if the expression is reached,
# making constant folding consistent with many other languages, and purely an
# optimisation rather than a behaviour change.


my $a;
$a = eval '$b = 0/0 if 0; 3';
is ($a, 3);
is ($@, "");

my $b = 0;
$a = eval 'if ($b) {return sqrt -3} 3';
is ($a, 3);
is ($@, "");

$a = eval q{
	$b = eval q{if ($b) {return log 0} 4};
 	is ($b, 4);
	is ($@, "");
	5;
};
is ($a, 5);
is ($@, "");

# warn and die hooks should be disabled during constant folding

{
    my $c = 0;
    local $SIG{__WARN__} = sub { $c++   };
    local $SIG{__DIE__}  = sub { $c+= 2 };
    eval q{
	is($c, 0, "premature warn/die: $c");
	my $x = "a"+5;
	is($c, 1, "missing warn hook");
	is($x, 5, "a+5");
	$c = 0;
	$x = 1/0;
    };
    like ($@, qr/division/, "eval caught division");
    is($c, 2, "missing die hook");
}
