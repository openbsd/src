#!./perl

# Add new tests to the end with format:
# ########
#
# # test description
# Test code
# EXPECT
# Warn or die msgs (if any) at - line 1234
#

chdir 't' if -d 't';
@INC = '../lib';
$ENV{PERL5LIB} = "../lib";

$|=1;

undef $/;
@prgs = split /^########\n/m, <DATA>;

require './test.pl';
plan(tests => scalar @prgs);
for (@prgs){
    ++$i;
    my($prog,$expected) = split(/\nEXPECT\n/, $_, 2);
    print("not ok $i # bad test format\n"), next
        unless defined $expected;
    my ($testname) = $prog =~ /^# (.*)\n/m;
    $testname ||= '';
    $TODO = $testname =~ s/^TODO //;
    $results =~ s/\n+$//;
    $expected =~ s/\n+$//;

    fresh_perl_is($prog, $expected, {}, $testname);
}

__END__

# standard behaviour, without any extra references
use Tie::Hash ;
tie %h, Tie::StdHash;
untie %h;
EXPECT
########

# standard behaviour, without any extra references
use Tie::Hash ;
{package Tie::HashUntie;
 use base 'Tie::StdHash';
 sub UNTIE
  {
   warn "Untied\n";
  }
}
tie %h, Tie::HashUntie;
untie %h;
EXPECT
Untied
########

# standard behaviour, with 1 extra reference
use Tie::Hash ;
$a = tie %h, Tie::StdHash;
untie %h;
EXPECT
########

# standard behaviour, with 1 extra reference via tied
use Tie::Hash ;
tie %h, Tie::StdHash;
$a = tied %h;
untie %h;
EXPECT
########

# standard behaviour, with 1 extra reference which is destroyed
use Tie::Hash ;
$a = tie %h, Tie::StdHash;
$a = 0 ;
untie %h;
EXPECT
########

# standard behaviour, with 1 extra reference via tied which is destroyed
use Tie::Hash ;
tie %h, Tie::StdHash;
$a = tied %h;
$a = 0 ;
untie %h;
EXPECT
########

# strict behaviour, without any extra references
use warnings 'untie';
use Tie::Hash ;
tie %h, Tie::StdHash;
untie %h;
EXPECT
########

# strict behaviour, with 1 extra references generating an error
use warnings 'untie';
use Tie::Hash ;
$a = tie %h, Tie::StdHash;
untie %h;
EXPECT
untie attempted while 1 inner references still exist at - line 6.
########

# strict behaviour, with 1 extra references via tied generating an error
use warnings 'untie';
use Tie::Hash ;
tie %h, Tie::StdHash;
$a = tied %h;
untie %h;
EXPECT
untie attempted while 1 inner references still exist at - line 7.
########

# strict behaviour, with 1 extra references which are destroyed
use warnings 'untie';
use Tie::Hash ;
$a = tie %h, Tie::StdHash;
$a = 0 ;
untie %h;
EXPECT
########

# strict behaviour, with extra 1 references via tied which are destroyed
use warnings 'untie';
use Tie::Hash ;
tie %h, Tie::StdHash;
$a = tied %h;
$a = 0 ;
untie %h;
EXPECT
########

# strict error behaviour, with 2 extra references
use warnings 'untie';
use Tie::Hash ;
$a = tie %h, Tie::StdHash;
$b = tied %h ;
untie %h;
EXPECT
untie attempted while 2 inner references still exist at - line 7.
########

# strict behaviour, check scope of strictness.
no warnings 'untie';
use Tie::Hash ;
$A = tie %H, Tie::StdHash;
$C = $B = tied %H ;
{
    use warnings 'untie';
    use Tie::Hash ;
    tie %h, Tie::StdHash;
    untie %h;
}
untie %H;
EXPECT
########

# Forbidden aggregate self-ties
sub Self::TIEHASH { bless $_[1], $_[0] }
{
    my %c;
    tie %c, 'Self', \%c;
}
EXPECT
Self-ties of arrays and hashes are not supported at - line 6.
########

# Allowed scalar self-ties
my $destroyed = 0;
sub Self::TIESCALAR { bless $_[1], $_[0] }
sub Self::DESTROY   { $destroyed = 1; }
{
    my $c = 42;
    tie $c, 'Self', \$c;
}
die "self-tied scalar not DESTROYed" unless $destroyed == 1;
EXPECT
########

# Allowed glob self-ties
my $destroyed = 0;
my $printed   = 0;
sub Self2::TIEHANDLE { bless $_[1], $_[0] }
sub Self2::DESTROY   { $destroyed = 1; }
sub Self2::PRINT     { $printed = 1; }
{
    use Symbol;
    my $c = gensym;
    tie *$c, 'Self2', $c;
    print $c 'Hello';
}
die "self-tied glob not PRINTed" unless $printed == 1;
die "self-tied glob not DESTROYed" unless $destroyed == 1;
EXPECT
########

# Allowed IO self-ties
my $destroyed = 0;
sub Self3::TIEHANDLE { bless $_[1], $_[0] }
sub Self3::DESTROY   { $destroyed = 1; }
sub Self3::PRINT     { $printed = 1; }
{
    use Symbol 'geniosym';
    my $c = geniosym;
    tie *$c, 'Self3', $c;
    print $c 'Hello';
}
die "self-tied IO not PRINTed" unless $printed == 1;
die "self-tied IO not DESTROYed" unless $destroyed == 1;
EXPECT
########

# TODO IO "self-tie" via TEMP glob
my $destroyed = 0;
sub Self3::TIEHANDLE { bless $_[1], $_[0] }
sub Self3::DESTROY   { $destroyed = 1; }
sub Self3::PRINT     { $printed = 1; }
{
    use Symbol 'geniosym';
    my $c = geniosym;
    tie *$c, 'Self3', \*$c;
    print $c 'Hello';
}
die "IO tied to TEMP glob not PRINTed" unless $printed == 1;
die "IO tied to TEMP glob not DESTROYed" unless $destroyed == 1;
EXPECT
########

# Interaction of tie and vec

my ($a, $b);
use Tie::Scalar;
tie $a,Tie::StdScalar or die;
vec($b,1,1)=1;
$a = $b;
vec($a,1,1)=0;
vec($b,1,1)=0;
die unless $a eq $b;
EXPECT
########

# correct unlocalisation of tied hashes (patch #16431)
use Tie::Hash ;
tie %tied, Tie::StdHash;
{ local $hash{'foo'} } warn "plain hash bad unlocalize" if exists $hash{'foo'};
{ local $tied{'foo'} } warn "tied hash bad unlocalize" if exists $tied{'foo'};
{ local $ENV{'foo'}  } warn "%ENV bad unlocalize" if exists $ENV{'foo'};
EXPECT
########

# An attempt at lvalueable barewords broke this
tie FH, 'main';
EXPECT
Can't modify constant item in tie at - line 3, near "'main';"
Execution of - aborted due to compilation errors.
########

# localizing tied hash slices
$ENV{FooA} = 1;
$ENV{FooB} = 2;
print exists $ENV{FooA} ? 1 : 0, "\n";
print exists $ENV{FooB} ? 2 : 0, "\n";
print exists $ENV{FooC} ? 3 : 0, "\n";
{
    local @ENV{qw(FooA FooC)};
    print exists $ENV{FooA} ? 4 : 0, "\n";
    print exists $ENV{FooB} ? 5 : 0, "\n";
    print exists $ENV{FooC} ? 6 : 0, "\n";
}
print exists $ENV{FooA} ? 7 : 0, "\n";
print exists $ENV{FooB} ? 8 : 0, "\n";
print exists $ENV{FooC} ? 9 : 0, "\n"; # this should not exist
EXPECT
1
2
0
4
5
6
7
8
0
########
#
# FETCH freeing tie'd SV
sub TIESCALAR { bless [] }
sub FETCH { *a = \1; 1 }
tie $a, 'main';
print $a;
EXPECT
########

#  [20020716.007] - nested FETCHES

sub F1::TIEARRAY { bless [], 'F1' }
sub F1::FETCH { 1 }
my @f1;
tie @f1, 'F1';

sub F2::TIEARRAY { bless [2], 'F2' }
sub F2::FETCH { my $self = shift; my $x = $f1[3]; $self }
my @f2;
tie @f2, 'F2';

print $f2[4][0],"\n";

sub F3::TIEHASH { bless [], 'F3' }
sub F3::FETCH { 1 }
my %f3;
tie %f3, 'F3';

sub F4::TIEHASH { bless [3], 'F4' }
sub F4::FETCH { my $self = shift; my $x = $f3{3}; $self }
my %f4;
tie %f4, 'F4';

print $f4{'foo'}[0],"\n";

EXPECT
2
3
########
# test untie() from within FETCH
package Foo;
sub TIESCALAR { my $pkg = shift; return bless [@_], $pkg; }
sub FETCH {
  my $self = shift;
  my ($obj, $field) = @$self;
  untie $obj->{$field};
  $obj->{$field} = "Bar";
}
package main;
tie $a->{foo}, "Foo", $a, "foo";
$a->{foo}; # access once
# the hash element should not be tied anymore
print defined tied $a->{foo} ? "not ok" : "ok";
EXPECT
ok
########
# the tmps returned by FETCH should appear to be SCALAR
# (even though they are now implemented using PVLVs.)
package X;
sub TIEHASH { bless {} }
sub TIEARRAY { bless {} }
sub FETCH {1}
my (%h, @a);
tie %h, 'X';
tie @a, 'X';
my $r1 = \$h{1};
my $r2 = \$a[0];
my $s = "$r1 ". ref($r1) . " $r2 " . ref($r2);
$s=~ s/\(0x\w+\)//g;
print $s, "\n";
EXPECT
SCALAR SCALAR SCALAR SCALAR
########
# [perl #23287] segfault in untie
sub TIESCALAR { bless $_[1], $_[0] }
my $var;
tie $var, 'main', \$var;
untie $var;
EXPECT
########
# Test case from perlmonks by runrig
# http://www.perlmonks.org/index.pl?node_id=273490
# "Here is what I tried. I think its similar to what you've tried
#  above. Its odd but convienient that after untie'ing you are left with
#  a variable that has the same value as was last returned from
#  FETCH. (At least on my perl v5.6.1). So you don't need to pass a
#  reference to the variable in order to set it after the untie (here it
#  is accessed through a closure)."
use strict;
use warnings;
package MyTied;
sub TIESCALAR {
    my ($class,$code) = @_;
    bless $code, $class;
}
sub FETCH {
    my $self = shift;
    print "Untie\n";
    $self->();
}
package main;
my $var;
tie $var, 'MyTied', sub { untie $var; 4 };
print "One\n";
print "$var\n";
print "Two\n";
print "$var\n";
print "Three\n";
print "$var\n";
EXPECT
One
Untie
4
Two
4
Three
4
########
# [perl #22297] cannot untie scalar from within tied FETCH
my $counter = 0;
my $x = 7;
my $ref = \$x;
tie $x, 'Overlay', $ref, $x;
my $y;
$y = $x;
$y = $x;
$y = $x;
$y = $x;
#print "WILL EXTERNAL UNTIE $ref\n";
untie $$ref;
$y = $x;
$y = $x;
$y = $x;
$y = $x;
#print "counter = $counter\n";

print (($counter == 1) ? "ok\n" : "not ok\n");

package Overlay;

sub TIESCALAR
{
        my $pkg = shift;
        my ($ref, $val) = @_;
        return bless [ $ref, $val ], $pkg;
}

sub FETCH
{
        my $self = shift;
        my ($ref, $val) = @$self;
        #print "WILL INTERNAL UNITE $ref\n";
        $counter++;
        untie $$ref;
        return $val;
}
EXPECT
ok
########

# [perl #948] cannot meaningfully tie $,
package TieDollarComma;

sub TIESCALAR {
     my $pkg = shift;
     return bless \my $x, $pkg;
}

sub STORE {
    my $self = shift;
    $$self = shift;
    print "STORE set '$$self'\n";
}

sub FETCH {
    my $self = shift;
    print "<FETCH>";
    return $$self;
}
package main;

tie $,, 'TieDollarComma';
$, = 'BOBBINS';
print "join", "things", "up\n";
EXPECT
STORE set 'BOBBINS'
join<FETCH>BOBBINSthings<FETCH>BOBBINSup
########

# test SCALAR method
package TieScalar;

sub TIEHASH {
    my $pkg = shift;
    bless { } => $pkg;
}

sub STORE {
    $_[0]->{$_[1]} = $_[2];
}

sub FETCH {
    $_[0]->{$_[1]}
}

sub CLEAR {
    %{ $_[0] } = ();
}

sub SCALAR {
    print "SCALAR\n";
    return 0 if ! keys %{$_[0]};
    sprintf "%i/%i", scalar keys %{$_[0]}, scalar keys %{$_[0]};
}

package main;
tie my %h => "TieScalar";
$h{key1} = "val1";
$h{key2} = "val2";
print scalar %h, "\n"
    if %h; # this should also call SCALAR but implicitly
%h = ();
print scalar %h, "\n"
    if !%h; # this should also call SCALAR but implicitly
EXPECT
SCALAR
SCALAR
2/2
SCALAR
SCALAR
0
########

# test scalar on tied hash when no SCALAR method has been given
package TieScalar;

sub TIEHASH {
    my $pkg = shift;
    bless { } => $pkg;
}
sub STORE {
    $_[0]->{$_[1]} = $_[2];
}
sub FETCH {
    $_[0]->{$_[1]}
}
sub CLEAR {
    %{ $_[0] } = ();
}
sub FIRSTKEY {
    my $a = keys %{ $_[0] };
    print "FIRSTKEY\n";
    each %{ $_[0] };
}

package main;
tie my %h => "TieScalar";

if (!%h) {
    print "empty\n";
} else {
    print "not empty\n";
}

$h{key1} = "val1";
print "not empty\n" if %h;
print "not empty\n" if %h;
print "-->\n";
my ($k,$v) = each %h;
print "<--\n";
print "not empty\n" if %h;
%h = ();
print "empty\n" if ! %h;
EXPECT
FIRSTKEY
empty
FIRSTKEY
not empty
FIRSTKEY
not empty
-->
FIRSTKEY
<--
not empty
FIRSTKEY
empty
########
sub TIESCALAR { bless {} }
sub FETCH { my $x = 3.3; 1 if 0+$x; $x }
tie $h, "main";
print $h,"\n";
EXPECT
3.3
########
sub TIESCALAR { bless {} }
sub FETCH { shift()->{i} ++ }
tie $h, "main";
print $h.$h;
EXPECT
01
########
# Bug 53482 (and maybe others)
sub TIESCALAR { my $foo = $_[1]; bless \$foo, $_[0] }
sub FETCH { ${$_[0]} }
tie my $x1, "main", 2;
tie my $y1, "main", 8;
print $x1 | $y1;
print $x1 | $y1;
tie my $x2, "main", "2";
tie my $y2, "main", "8";
print $x2 | $y2;
print $x2 | $y2;
EXPECT
1010::
########
# Bug 36267
sub TIEHASH  { bless {}, $_[0] }
sub STORE    { $_[0]->{$_[1]} = $_[2] }
sub FIRSTKEY { my $a = scalar keys %{$_[0]}; each %{$_[0]} }
sub NEXTKEY  { each %{$_[0]} }
sub DELETE   { delete $_[0]->{$_[1]} }
sub CLEAR    { %{$_[0]} = () }
$h{b}=1;
delete $h{b};
print scalar keys %h, "\n";
tie %h, 'main';
$i{a}=1;
%h = %i;
untie %h;
print scalar keys %h, "\n";
EXPECT
0
0
########
# Bug 37731
sub foo::TIESCALAR { bless {value => $_[1]}, $_[0] }
sub foo::FETCH { $_[0]->{value} }
tie my $VAR, 'foo', '42';
foreach my $var ($VAR) {
    print +($var eq $VAR) ? "yes\n" : "no\n";
}
EXPECT
yes
########
sub TIEARRAY { bless [], 'main' }
{
    local @a;
    tie @a, 'main';
}
print "tied\n" if tied @a;
EXPECT
########
sub TIEHASH { bless [], 'main' }
{
    local %h;
    tie %h, 'main';
}
print "tied\n" if tied %h;
EXPECT
