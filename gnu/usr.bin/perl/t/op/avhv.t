#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use warnings;
no warnings 'deprecated';
use strict;
use vars qw(@fake %fake);

require Tie::Array;

package Tie::BasicArray;
@Tie::BasicArray::ISA = 'Tie::Array';
sub TIEARRAY  { bless [], $_[0] }
sub STORE     { $_[0]->[$_[1]] = $_[2] }
sub FETCH     { $_[0]->[$_[1]] }
sub FETCHSIZE { scalar(@{$_[0]})} 
sub STORESIZE { $#{$_[0]} = $_[1]+1 } 

package main;

plan tests => 36;

my $sch = {
    'abc' => 1,
    'def' => 2,
    'jkl' => 3,
};

# basic normal array
$a = [];
$a->[0] = $sch;

$a->{'abc'} = 'ABC';
$a->{'def'} = 'DEF';
$a->{'jkl'} = 'JKL';

my @keys = keys %$a;
my @values = values %$a;

is ($#keys, 2);
is ($#values, 2);

my $i = 0;	# stop -w complaints

while (my ($key,$value) = each %$a) {
    if ($key eq $keys[$i] && $value eq $values[$i] && $key eq lc($value)) {
	$key =~ y/a-z/A-Z/;
	$i++ if $key eq $value;
    }
}

is ($i, 3);

# quick check with tied array
tie @fake, 'Tie::StdArray';
$a = \@fake;
$a->[0] = $sch;

$a->{'abc'} = 'ABC';
is ($a->{'abc'}, 'ABC');

# quick check with tied array
tie @fake, 'Tie::BasicArray';
$a = \@fake;
$a->[0] = $sch;

$a->{'abc'} = 'ABC';
is ($a->{'abc'}, 'ABC');

# quick check with tied array & tied hash
require Tie::Hash;
tie %fake, 'Tie::StdHash';
%fake = %$sch;
$a->[0] = \%fake;

$a->{'abc'} = 'ABC';
is ($a->{'abc'}, 'ABC');

# hash slice
{
  no warnings 'uninitialized';
  my $slice = join('', 'x',@$a{'abc','def'},'x');
  is ($slice, 'xABCx');
}

# evaluation in scalar context
my $avhv = [{}];
ok (!%$avhv);

push @$avhv, "a";
ok (!%$avhv);

$avhv = [];
eval { $a = %$avhv };
like ($@, qr/^Can't coerce array into hash/);

$avhv = [{foo=>1, bar=>2}];
like (%$avhv, qr,^\d+/\d+,);

# check if defelem magic works
sub f {
    is ($_[0], 'a');
    $_[0] = 'b';
}
$a = [{key => 1}, 'a'];
f($a->{key});
is ($a->[1], 'b');

# check if exists() is behaving properly
$avhv = [{foo=>1,bar=>2,pants=>3}];
ok (!exists $avhv->{bar});

$avhv->{pants} = undef;
ok (exists $avhv->{pants});
ok (!exists $avhv->{bar});

$avhv->{bar} = 10;
ok (exists $avhv->{bar});
is ($avhv->{bar}, 10);

my $v = delete $avhv->{bar};
is ($v, 10);

ok (!exists $avhv->{bar});

$avhv->{foo} = 'xxx';
$avhv->{bar} = 'yyy';
$avhv->{pants} = 'zzz';
my @x = delete @{$avhv}{'foo','pants'};
is ("@x", "xxx zzz");

is ("$avhv->{bar}", "yyy");

# hash assignment
%$avhv = ();
is (ref($avhv->[0]), 'HASH');

my %hv = %$avhv;
ok (!grep defined, values %hv);
ok (!grep ref, keys %hv);

%$avhv = (foo => 29, pants => 2, bar => 0);
is ("@$avhv[1..3]", '29 0 2');

my $extra;
my @extra;
($extra, %$avhv) = ("moo", foo => 42, pants => 53, bar => "HIKE!");
is ("@$avhv[1..3]", '42 HIKE! 53');
is ($extra, 'moo');

%$avhv = ();
(%$avhv, $extra) = (foo => 42, pants => 53, bar => "HIKE!");
is ("@$avhv[1..3]", '42 HIKE! 53');
ok (!defined $extra);

@extra = qw(whatever and stuff);
%$avhv = ();
(%$avhv, @extra) = (foo => 42, pants => 53, bar => "HIKE!");
is ("@$avhv[1..3]", '42 HIKE! 53');
is (@extra, 0);

%$avhv = ();
(@extra, %$avhv) = (foo => 42, pants => 53, bar => "HIKE!");
is (ref $avhv->[0], 'HASH');
is (@extra, 6);

# Check hash slices (BUG ID 20010423.002)
$avhv = [{foo=>1, bar=>2}];
@$avhv{"foo", "bar"} = (42, 53);
is ($avhv->{foo}, 42);
is ($avhv->{bar}, 53);
