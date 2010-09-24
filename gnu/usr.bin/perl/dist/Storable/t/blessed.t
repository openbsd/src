#!./perl
#
#  Copyright (c) 1995-2000, Raphael Manfredi
#  
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#

sub BEGIN {
    unshift @INC, 't';
    require Config; import Config;
    if ($ENV{PERL_CORE} and $Config{'extensions'} !~ /\bStorable\b/) {
        print "1..0 # Skip: Storable was not built\n";
        exit 0;
    }
    require 'st-dump.pl';
}

sub ok;

use Storable qw(freeze thaw);

%::immortals
  = (u => \undef,
     'y' => \(1 == 1),
     n => \(1 == 0)
);

my $test = 12;
my $tests = $test + 6 + 2 * 6 * keys %::immortals;
print "1..$tests\n";

package SHORT_NAME;

sub make { bless [], shift }

package SHORT_NAME_WITH_HOOK;

sub make { bless [], shift }

sub STORABLE_freeze {
	my $self = shift;
	return ("", $self);
}

sub STORABLE_thaw {
	my $self = shift;
	my $cloning = shift;
	my ($x, $obj) = @_;
	die "STORABLE_thaw" unless $obj eq $self;
}

package main;

# Still less than 256 bytes, so long classname logic not fully exercised
# Wait until Perl removes the restriction on identifier lengths.
my $name = "LONG_NAME_" . 'xxxxxxxxxxxxx::' x 14 . "final";

eval <<EOC;
package $name;

\@ISA = ("SHORT_NAME");
EOC
die $@ if $@;
ok 1, $@ eq '';

eval <<EOC;
package ${name}_WITH_HOOK;

\@ISA = ("SHORT_NAME_WITH_HOOK");
EOC
ok 2, $@ eq '';

# Construct a pool of objects
my @pool;

for (my $i = 0; $i < 10; $i++) {
	push(@pool, SHORT_NAME->make);
	push(@pool, SHORT_NAME_WITH_HOOK->make);
	push(@pool, $name->make);
	push(@pool, "${name}_WITH_HOOK"->make);
}

my $x = freeze \@pool;
ok 3, 1;

my $y = thaw $x;
ok 4, ref $y eq 'ARRAY';
ok 5, @{$y} == @pool;

ok 6, ref $y->[0] eq 'SHORT_NAME';
ok 7, ref $y->[1] eq 'SHORT_NAME_WITH_HOOK';
ok 8, ref $y->[2] eq $name;
ok 9, ref $y->[3] eq "${name}_WITH_HOOK";

my $good = 1;
for (my $i = 0; $i < 10; $i++) {
	do { $good = 0; last } unless ref $y->[4*$i]   eq 'SHORT_NAME';
	do { $good = 0; last } unless ref $y->[4*$i+1] eq 'SHORT_NAME_WITH_HOOK';
	do { $good = 0; last } unless ref $y->[4*$i+2] eq $name;
	do { $good = 0; last } unless ref $y->[4*$i+3] eq "${name}_WITH_HOOK";
}
ok 10, $good;

{
	my $blessed_ref = bless \\[1,2,3], 'Foobar';
	my $x = freeze $blessed_ref;
	my $y = thaw $x;
	ok 11, ref $y eq 'Foobar';
	ok 12, $$$y->[0] == 1;
}

package RETURNS_IMMORTALS;

sub make { my $self = shift; bless [@_], $self }

sub STORABLE_freeze {
  # Some reference some number of times.
  my $self = shift;
  my ($what, $times) = @$self;
  return ("$what$times", ($::immortals{$what}) x $times);
}

sub STORABLE_thaw {
	my $self = shift;
	my $cloning = shift;
	my ($x, @refs) = @_;
	my ($what, $times) = $x =~ /(.)(\d+)/;
	die "'$x' didn't match" unless defined $times;
	main::ok ++$test, @refs == $times;
	my $expect = $::immortals{$what};
	die "'$x' did not give a reference" unless ref $expect;
	my $fail;
	foreach (@refs) {
	  $fail++ if $_ != $expect;
	}
	main::ok ++$test, !$fail;
}

package main;

# $Storable::DEBUGME = 1;
my $count;
foreach $count (1..3) {
  my $immortal;
  foreach $immortal (keys %::immortals) {
    print "# $immortal x $count\n";
    my $i =  RETURNS_IMMORTALS->make ($immortal, $count);

    my $f = freeze ($i);
    ok ++$test, $f;
    my $t = thaw $f;
    ok ++$test, 1;
  }
}

# Test automatic require of packages to find thaw hook.

package HAS_HOOK;

$loaded_count = 0;
$thawed_count = 0;

sub make {
  bless [];
}

sub STORABLE_freeze {
  my $self = shift;
  return '';
}

package main;

my $f = freeze (HAS_HOOK->make);

ok ++$test, $HAS_HOOK::loaded_count == 0;
ok ++$test, $HAS_HOOK::thawed_count == 0;

my $t = thaw $f;
ok ++$test, $HAS_HOOK::loaded_count == 1;
ok ++$test, $HAS_HOOK::thawed_count == 1;
ok ++$test, $t;
ok ++$test, ref $t eq 'HAS_HOOK';

# Can't do this because the method is still cached by UNIVERSAL::can
# delete $INC{"HAS_HOOK.pm"};
# undef &HAS_HOOK::STORABLE_thaw;
# 
# warn HAS_HOOK->can('STORABLE_thaw');
# $t = thaw $f;
# ok ++$test, $HAS_HOOK::loaded_count == 2;
# ok ++$test, $HAS_HOOK::thawed_count == 2;
# ok ++$test, $t;
# ok ++$test, ref $t eq 'HAS_HOOK';
