#!perl

# Basic tests for $[ as a variable
# plus miscellaneous bug fix tests

no warnings 'deprecated';
use Test::More tests => 7;

sub outside_base_scope { return "${'['}" }

$[ = 3;
my $base = \$[;
is "$$base", 3, 'retval of $[';
is outside_base_scope, 0, 'retval of $[ outside its scope';

${'['} = 3;
pass('run-time $[ = 3 assignment (in $[ = 3 scope)');
{
  $[ = 0;
  ${'['} = 0;
  pass('run-time $[ = 0 assignment (in $[ = 3 scope)');
}

eval { ${'['} = 1 }; my $f = __FILE__; my $l = __LINE__;
is $@, "That use of \$[ is unsupported at $f line $l.\n",
   "error when setting $[ to integer other than current base at run-time";

$[ = 6.7;
is "$[", 6, '$[ is an integer';

eval { my $x = 45; $[ = \$x }; $l = __LINE__;
is $@, "That use of \$[ is unsupported at $f line $l.\n",
   'error when setting $[ to ref';

sub foo { my $x; $x = wait } # compilation of this routine used to crash

1;
