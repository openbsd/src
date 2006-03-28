#!./perl
#
# check UNIVERSAL
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    $| = 1;
    require "./test.pl";
}

print "1..102\n";

$a = {};
bless $a, "Bob";
ok $a->isa("Bob");

package Human;
sub eat {}

package Female;
@ISA=qw(Human);

package Alice;
@ISA=qw(Bob Female);
sub sing;
sub drink { return "drinking " . $_[1]  }
sub new { bless {} }

$Alice::VERSION = 2.718;

{
    package Cedric;
    our @ISA;
    use base qw(Human);
}

{
    package Programmer;
    our $VERSION = 1.667;

    sub write_perl { 1 }
}

package main;



$a = new Alice;

ok $a->isa("Alice");
ok $a->isa("main::Alice");    # check that alternate class names work

ok(("main::Alice"->new)->isa("Alice"));

ok $a->isa("Bob");
ok $a->isa("main::Bob");

ok $a->isa("Female");

ok $a->isa("Human");

ok ! $a->isa("Male");

ok ! $a->isa('Programmer');

ok $a->isa("HASH");

ok $a->can("eat");
ok ! $a->can("sleep");
ok my $ref = $a->can("drink");        # returns a coderef
is $a->$ref("tea"), "drinking tea"; # ... which works
ok $ref = $a->can("sing");
eval { $a->$ref() };
ok $@;                                # ... but not if no actual subroutine

ok (!Cedric->isa('Programmer'));

ok (Cedric->isa('Human'));

push(@Cedric::ISA,'Programmer');

ok (Cedric->isa('Programmer'));

{
    package Alice;
    base::->import('Programmer');
}

ok $a->isa('Programmer');
ok $a->isa("Female");

@Cedric::ISA = qw(Bob);

ok (!Cedric->isa('Programmer'));

my $b = 'abc';
my @refs = qw(SCALAR SCALAR     LVALUE      GLOB ARRAY HASH CODE);
my @vals = (  \$b,   \3.14, \substr($b,1,1), \*b,  [],  {}, sub {} );
for ($p=0; $p < @refs; $p++) {
    for ($q=0; $q < @vals; $q++) {
        is UNIVERSAL::isa($vals[$p], $refs[$q]), ($p==$q or $p+$q==1);
    };
};

ok ! UNIVERSAL::can(23, "can");

ok $a->can("VERSION");

ok $a->can("can");
ok ! $a->can("export_tags");	# a method in Exporter

cmp_ok eval { $a->VERSION }, '==', 2.718;

ok ! (eval { $a->VERSION(2.719) });
like $@, qr/^Alice version 2.71(?:9|8999\d+) required--this is only version 2.718 at /;

ok (eval { $a->VERSION(2.718) });
is $@, '';

my $subs = join ' ', sort grep { defined &{"UNIVERSAL::$_"} } keys %UNIVERSAL::;
## The test for import here is *not* because we want to ensure that UNIVERSAL
## can always import; it is an historical accident that UNIVERSAL can import.
if ('a' lt 'A') {
    is $subs, "can import isa VERSION";
} else {
    is $subs, "VERSION can import isa";
}

ok $a->isa("UNIVERSAL");

ok ! UNIVERSAL::isa([], "UNIVERSAL");

ok ! UNIVERSAL::can({}, "can");

ok UNIVERSAL::isa(Alice => "UNIVERSAL");

cmp_ok UNIVERSAL::can(Alice => "can"), '==', \&UNIVERSAL::can;

# now use UNIVERSAL.pm and see what changes
eval "use UNIVERSAL";

ok $a->isa("UNIVERSAL");

my $sub2 = join ' ', sort grep { defined &{"UNIVERSAL::$_"} } keys %UNIVERSAL::;
# XXX import being here is really a bug
if ('a' lt 'A') {
    is $sub2, "can import isa VERSION";
} else {
    is $sub2, "VERSION can import isa";
}

eval 'sub UNIVERSAL::sleep {}';
ok $a->can("sleep");

ok ! UNIVERSAL::can($b, "can");

ok ! $a->can("export_tags");	# a method in Exporter

ok ! UNIVERSAL::isa("\xff\xff\xff\0", 'HASH');

{
    package Pickup;
    use UNIVERSAL qw( isa can VERSION );

    ::ok isa "Pickup", UNIVERSAL;
    ::cmp_ok can( "Pickup", "can" ), '==', \&UNIVERSAL::can;
    ::ok VERSION "UNIVERSAL" ;
}

{
    # test isa() and can() on magic variables
    "Human" =~ /(.*)/;
    ok $1->isa("Human");
    ok $1->can("eat");
    package HumanTie;
    sub TIESCALAR { bless {} }
    sub FETCH { "Human" }
    tie my($x), "HumanTie";
    ::ok $x->isa("Human");
    ::ok $x->can("eat");
}

# bugid 3284
# a second call to isa('UNIVERSAL') when @ISA is null failed due to caching

@X::ISA=();
my $x = {}; bless $x, 'X';
ok $x->isa('UNIVERSAL');
ok $x->isa('UNIVERSAL');
