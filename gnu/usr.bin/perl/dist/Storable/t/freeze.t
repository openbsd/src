#!./perl
#
#  Copyright (c) 1995-2000, Raphael Manfredi
#
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#

use strict;
use warnings;

sub BEGIN {
    unshift @INC, 't/lib';
}

use STDump;
use Storable qw(freeze nfreeze thaw);

$Storable::flags = Storable::FLAGS_COMPAT;

use Test::More tests => 21;

my $a = 'toto';
my $b = \$a;
my $c = bless {}, 'CLASS';
$c->{attribute} = $b;
my $d = {};
my $e = [];
$d->{'a'} = $e;
$e->[0] = $d;
my %a = ('key', 'value', 1, 0, $a, $b, 'cvar', \$c);
my @a = ('first', undef, 3, -4, -3.14159, 456, 4.5, $d, \$d, \$e, $e,
    $b, \$a, $a, $c, \$c, \%a);

my $f1 = freeze(\@a);
isnt($f1, undef);

my $dumped = stdump(\@a);
isnt($dumped, undef);

my $root = thaw($f1);
isnt($root, undef);

my $got = stdump($root);
isnt($got, undef);

is($got, $dumped);

package FOO; our @ISA = qw(Storable);

sub make {
    my $self = bless {};
    $self->{key} = \%a;
    return $self;
};

package main;

my $foo = FOO->make;
my $f2 = $foo->freeze;
isnt($f2, undef);

my $f3 = $foo->nfreeze;
isnt($f3, undef);

my $root3 = thaw($f3);
isnt($root3, undef);

is(stdump($foo), stdump($root3));

$root = thaw($f2);
is(stdump($foo), stdump($root));

is(stdump($root3), stdump($root));

my $other = freeze($root);
is(length$other, length $f2);

my $root2 = thaw($other);
is(stdump($root2), stdump($root));

my $VAR1 = [
    'method',
    1,
    'prepare',
    'SELECT table_name, table_owner, num_rows FROM iitables
        where table_owner != \'$ingres\' and table_owner != \'DBA\''
];

my $x = nfreeze($VAR1);
my $VAR2 = thaw($x);
is($VAR2->[3], $VAR1->[3]);

# Test the workaround for LVALUE bug in perl 5.004_04 -- from Gisle Aas
sub foo { $_[0] = 1 }
$foo = [];
foo($foo->[1]);
eval { freeze($foo) };
is($@, '');

# Test cleanup bug found by Claudio Garcia -- RAM, 08/06/2001
my $thaw_me = 'asdasdasdasd';

eval {
    my $thawed = thaw $thaw_me;
};
isnt($@, '');

my %to_be_frozen = (foo => 'bar');
my $frozen;
eval {
    $frozen = freeze \%to_be_frozen;
};
is($@, '');

freeze {};
eval { thaw $thaw_me };
eval { $frozen = freeze { foo => {} } };
is($@, '');

thaw $frozen;                   # used to segfault here
pass("Didn't segfault");

SKIP: {
    my (@a, @b);
    eval '
        $a = []; $#$a = 2; $a->[1] = undef;
        $b = thaw freeze $a;
        @a = map { ~~ exists $a->[$_] } 0 .. $#$a;
        @b = map { ~~ exists $b->[$_] } 0 .. $#$b;
    ';
    is($@, '');
    is("@a", "@b");
}
