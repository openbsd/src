#!./perl
#
# check UNIVERSAL
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    $| = 1;
}

print "1..94\n";

$a = {};
bless $a, "Bob";
print "not " unless $a->isa("Bob");
print "ok 1\n";

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

{ my $i = 2;
  sub test {
      print "not " unless $_[0];
      print "ok ", $i++;
      print " # at ", (caller)[1], ", line ", (caller)[2] unless $_[0];
      print "\n";
  }
}

$a = new Alice;

test $a->isa("Alice");
test $a->isa("main::Alice");    # check that alternate class names work

test(("main::Alice"->new)->isa("Alice"));

test $a->isa("Bob");
test $a->isa("main::Bob");

test $a->isa("Female");

test $a->isa("Human");

test ! $a->isa("Male");

test ! $a->isa('Programmer');

test $a->isa("HASH");

test $a->can("eat");
test ! $a->can("sleep");
test my $ref = $a->can("drink");        # returns a coderef
test $a->$ref("tea") eq "drinking tea"; # ... which works
test $ref = $a->can("sing");
eval { $a->$ref() };
test $@;                                # ... but not if no actual subroutine

test (!Cedric->isa('Programmer'));

test (Cedric->isa('Human'));

push(@Cedric::ISA,'Programmer');

test (Cedric->isa('Programmer'));

{
    package Alice;
    base::->import('Programmer');
}

test $a->isa('Programmer');
test $a->isa("Female");

@Cedric::ISA = qw(Bob);

test (!Cedric->isa('Programmer'));

my $b = 'abc';
my @refs = qw(SCALAR SCALAR     LVALUE      GLOB ARRAY HASH CODE);
my @vals = (  \$b,   \3.14, \substr($b,1,1), \*b,  [],  {}, sub {} );
for ($p=0; $p < @refs; $p++) {
    for ($q=0; $q < @vals; $q++) {
        test UNIVERSAL::isa($vals[$p], $refs[$q]) eq ($p==$q or $p+$q==1);
    };
};

test ! UNIVERSAL::can(23, "can");

test $a->can("VERSION");

test $a->can("can");
test ! $a->can("export_tags");	# a method in Exporter

test (eval { $a->VERSION }) == 2.718;

test ! (eval { $a->VERSION(2.719) }) &&
         $@ =~ /^Alice version 2.71(?:9|8999\d+) required--this is only version 2.718 at /;

test (eval { $a->VERSION(2.718) }) && ! $@;

my $subs = join ' ', sort grep { defined &{"UNIVERSAL::$_"} } keys %UNIVERSAL::;
## The test for import here is *not* because we want to ensure that UNIVERSAL
## can always import; it is an historical accident that UNIVERSAL can import.
if ('a' lt 'A') {
    test $subs eq "can import isa VERSION";
} else {
    test $subs eq "VERSION can import isa";
}

test $a->isa("UNIVERSAL");

test ! UNIVERSAL::isa([], "UNIVERSAL");

test ! UNIVERSAL::can({}, "can");

test UNIVERSAL::isa(Alice => "UNIVERSAL");

test UNIVERSAL::can(Alice => "can") == \&UNIVERSAL::can;

# now use UNIVERSAL.pm and see what changes
eval "use UNIVERSAL";

test $a->isa("UNIVERSAL");

my $sub2 = join ' ', sort grep { defined &{"UNIVERSAL::$_"} } keys %UNIVERSAL::;
# XXX import being here is really a bug
if ('a' lt 'A') {
    test $sub2 eq "can import isa VERSION";
} else {
    test $sub2 eq "VERSION can import isa";
}

eval 'sub UNIVERSAL::sleep {}';
test $a->can("sleep");

test ! UNIVERSAL::can($b, "can");

test ! $a->can("export_tags");	# a method in Exporter

test ! UNIVERSAL::isa("\xff\xff\xff\0", 'HASH');

{
    package Pickup;
    use UNIVERSAL qw( isa can VERSION );

    main::test isa "Pickup", UNIVERSAL;
    main::test can( "Pickup", "can" ) == \&UNIVERSAL::can;
    main::test VERSION "UNIVERSAL" ;
}
