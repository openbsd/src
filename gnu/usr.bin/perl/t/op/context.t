#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib);
}

require "test.pl";
plan( tests => 7 );

sub foo {
    $a='abcd';
    $a=~/(.)/g;
    cmp_ok($1,'eq','a','context ' . curr_test());
}

$a=foo;
@a=foo;
foo;
foo(foo);

my $before = curr_test();
$h{foo} = foo;
my $after = curr_test();

cmp_ok($after-$before,'==',1,'foo called once')
	or diag("nr tests: before=$before, after=$after");
