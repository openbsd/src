#!./perl

# Test || in weird situations.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}


package Countdown;

sub TIESCALAR {
  my $class = shift;
  my $instance = shift || undef;
  return bless \$instance => $class;
}

sub FETCH {
  print "# FETCH!  ${$_[0]}\n";
  return ${$_[0]}--;
}


package main;
require './test.pl';

plan( tests => 8 );


my ($a, $b, $c);

$! = 1;
$a = $!;
my $a_str = sprintf "%s", $a;
my $a_num = sprintf "%d", $a;

$c = $a || $b;

is($c, $a_str);
is($c+0, $a_num);   # force numeric context.

$a =~ /./g or die "Match failed for some reason"; # Make $a magic

$c = $a || $b;

is($c, $a_str);
is($c+0, $a_num);   # force numeric context.

my $val = 3;

$c = $val || $b;
is($c, 3);

tie $a, 'Countdown', $val;

$c = $a;
is($c, 3,       'Single FETCH on tied scalar');

$c = $a;
is($c, 2,       '   $tied = $var');

$c = $a || $b;

{
    local $TODO = 'Double FETCH';
    is($c, 1,   '   $tied || $var');
}
