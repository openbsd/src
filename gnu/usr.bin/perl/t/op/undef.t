#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;

use vars qw(@ary %ary %hash);

plan 37;

ok !defined($a);

$a = 1+1;
ok defined($a);

undef $a;
ok !defined($a);

$a = "hi";
ok defined($a);

$a = $b;
ok !defined($a);

@ary = ("1arg");
$a = pop(@ary);
ok defined($a);
$a = pop(@ary);
ok !defined($a);

@ary = ("1arg");
$a = shift(@ary);
ok defined($a);
$a = shift(@ary);
ok !defined($a);

$ary{'foo'} = 'hi';
ok defined($ary{'foo'});
ok !defined($ary{'bar'});
undef $ary{'foo'};
ok !defined($ary{'foo'});

ok defined(@ary);
ok defined(%ary);
undef @ary;
ok !defined(@ary);
undef %ary;
ok !defined(%ary);
@ary = (1);
ok defined @ary;
%ary = (1,1);
ok defined %ary;

sub foo { pass; 1 }

&foo || fail;

ok defined &foo;
undef &foo;
ok !defined(&foo);

eval { undef $1 };
like $@, qr/^Modification of a read/;

eval { $1 = undef };
like $@, qr/^Modification of a read/;

{
    require Tie::Hash;
    tie my %foo, 'Tie::StdHash';
    ok defined %foo;
    %foo = ( a => 1 );
    ok defined %foo;
}

{
    require Tie::Array;
    tie my @foo, 'Tie::StdArray';
    ok defined @foo;
    @foo = ( a => 1 );
    ok defined @foo;
}

{
    # [perl #17753] segfault when undef'ing unquoted string constant
    eval 'undef tcp';
    like $@, qr/^Can't modify constant item/;
}

# bugid 3096
# undefing a hash may free objects with destructors that then try to
# modify the hash. To them, the hash should appear empty.

%hash = (
    key1 => bless({}, 'X'),
    key2 => bless({}, 'X'),
);
undef %hash;
sub X::DESTROY {
    is scalar keys %hash, 0;
    is scalar values %hash, 0;
    my @l = each %hash;
    is @l, 0;
    is delete $hash{'key2'}, undef;
}

# this will segfault if it fails

sub PVBM () { 'foo' }
{ my $dummy = index 'foo', PVBM }

my $pvbm = PVBM;
undef $pvbm;
ok !defined $pvbm;
