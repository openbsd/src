#!./perl

#
# test method calls and autoloading.
#

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib);
    require "test.pl";
}

print "1..75\n";

@A::ISA = 'B';
@B::ISA = 'C';

sub C::d {"C::d"}
sub D::d {"D::d"}

# First, some basic checks of method-calling syntax:
$obj = bless [], "Pack";
sub Pack::method { shift; join(",", "method", @_) }
$mname = "method";

is(Pack->method("a","b","c"), "method,a,b,c");
is(Pack->$mname("a","b","c"), "method,a,b,c");
is(method Pack ("a","b","c"), "method,a,b,c");
is((method Pack "a","b","c"), "method,a,b,c");

is(Pack->method(), "method");
is(Pack->$mname(), "method");
is(method Pack (), "method");
is(Pack->method, "method");
is(Pack->$mname, "method");
is(method Pack, "method");

is($obj->method("a","b","c"), "method,a,b,c");
is($obj->$mname("a","b","c"), "method,a,b,c");
is((method $obj ("a","b","c")), "method,a,b,c");
is((method $obj "a","b","c"), "method,a,b,c");

is($obj->method(0), "method,0");
is($obj->method(1), "method,1");

is($obj->method(), "method");
is($obj->$mname(), "method");
is((method $obj ()), "method");
is($obj->method, "method");
is($obj->$mname, "method");
is(method $obj, "method");

is( A->d, "C::d");		# Update hash table;

*B::d = \&D::d;			# Import now.
is(A->d, "D::d");		# Update hash table;

{
    local @A::ISA = qw(C);	# Update hash table with split() assignment
    is(A->d, "C::d");
    $#A::ISA = -1;
    is(eval { A->d } || "fail", "fail");
}
is(A->d, "D::d");

{
    local *B::d;
    eval 'sub B::d {"B::d1"}';	# Import now.
    is(A->d, "B::d1");	# Update hash table;
    undef &B::d;
    is((eval { A->d }, ($@ =~ /Undefined subroutine/)), 1);
}

is(A->d, "D::d");		# Back to previous state

eval 'sub B::d {"B::d2"}';	# Import now.
is(A->d, "B::d2");		# Update hash table;

# What follows is hardly guarantied to work, since the names in scripts
# are already linked to "pruned" globs. Say, `undef &B::d' if it were
# after `delete $B::{d}; sub B::d {}' would reach an old subroutine.

undef &B::d;
delete $B::{d};
is(A->d, "C::d");		# Update hash table;

eval 'sub B::d {"B::d3"}';	# Import now.
is(A->d, "B::d3");		# Update hash table;

delete $B::{d};
*dummy::dummy = sub {};		# Mark as updated
is(A->d, "C::d");

eval 'sub B::d {"B::d4"}';	# Import now.
is(A->d, "B::d4");		# Update hash table;

delete $B::{d};			# Should work without any help too
is(A->d, "C::d");

{
    local *C::d;
    is(eval { A->d } || "nope", "nope");
}
is(A->d, "C::d");

*A::x = *A::d;			# See if cache incorrectly follows synonyms
A->d;
is(eval { A->x } || "nope", "nope");

eval <<'EOF';
sub C::e;
BEGIN { *B::e = \&C::e }	# Shouldn't prevent AUTOLOAD in original pkg
sub Y::f;
$counter = 0;

@X::ISA = 'Y';
@Y::ISA = 'B';

sub B::AUTOLOAD {
  my $c = ++$counter;
  my $method = $B::AUTOLOAD; 
  my $msg = "B: In $method, $c";
  eval "sub $method { \$msg }";
  goto &$method;
}
sub C::AUTOLOAD {
  my $c = ++$counter;
  my $method = $C::AUTOLOAD; 
  my $msg = "C: In $method, $c";
  eval "sub $method { \$msg }";
  goto &$method;
}
EOF

is(A->e(), "C: In C::e, 1");	# We get a correct autoload
is(A->e(), "C: In C::e, 1");	# Which sticks

is(A->ee(), "B: In A::ee, 2"); # We get a generic autoload, method in top
is(A->ee(), "B: In A::ee, 2"); # Which sticks

is(Y->f(), "B: In Y::f, 3");	# We vivify a correct method
is(Y->f(), "B: In Y::f, 3");	# Which sticks

# This test is not intended to be reasonable. It is here just to let you
# know that you broke some old construction. Feel free to rewrite the test
# if your patch breaks it.

*B::AUTOLOAD = sub {
  my $c = ++$counter;
  my $method = $AUTOLOAD; 
  *$AUTOLOAD = sub { "new B: In $method, $c" };
  goto &$AUTOLOAD;
};

is(A->eee(), "new B: In A::eee, 4");	# We get a correct $autoload
is(A->eee(), "new B: In A::eee, 4");	# Which sticks

# this test added due to bug discovery
is(defined(@{"unknown_package::ISA"}) ? "defined" : "undefined", "undefined");

# test that failed subroutine calls don't affect method calls
{
    package A1;
    sub foo { "foo" }
    package A2;
    @ISA = 'A1';
    package main;
    is(A2->foo(), "foo");
    is(do { eval 'A2::foo()'; $@ ? 1 : 0}, 1);
    is(A2->foo(), "foo");
}

## This test was totally misguided.  It passed before only because the
## code to determine if a package was loaded used to look for the hash
## %Foo::Bar instead of the package Foo::Bar:: -- and Config.pm just
## happens to export %Config.
#  {
#      is(do { use Config; eval 'Config->foo()';
#  	      $@ =~ /^\QCan't locate object method "foo" via package "Config" at/ ? 1 : $@}, 1);
#      is(do { use Config; eval '$d = bless {}, "Config"; $d->foo()';
#  	      $@ =~ /^\QCan't locate object method "foo" via package "Config" at/ ? 1 : $@}, 1);
#  }


# test error messages if method loading fails
is(do { eval '$e = bless {}, "E::A"; E::A->foo()';
	  $@ =~ /^\QCan't locate object method "foo" via package "E::A" at/ ? 1 : $@}, 1);
is(do { eval '$e = bless {}, "E::B"; $e->foo()';  
	  $@ =~ /^\QCan't locate object method "foo" via package "E::B" at/ ? 1 : $@}, 1);
is(do { eval 'E::C->foo()';
	  $@ =~ /^\QCan't locate object method "foo" via package "E::C" (perhaps / ? 1 : $@}, 1);

is(do { eval 'UNIVERSAL->E::D::foo()';
	  $@ =~ /^\QCan't locate object method "foo" via package "E::D" (perhaps / ? 1 : $@}, 1);
is(do { eval '$e = bless {}, "UNIVERSAL"; $e->E::E::foo()';
	  $@ =~ /^\QCan't locate object method "foo" via package "E::E" (perhaps / ? 1 : $@}, 1);

$e = bless {}, "E::F";  # force package to exist
is(do { eval 'UNIVERSAL->E::F::foo()';
	  $@ =~ /^\QCan't locate object method "foo" via package "E::F" at/ ? 1 : $@}, 1);
is(do { eval '$e = bless {}, "UNIVERSAL"; $e->E::F::foo()';
	  $@ =~ /^\QCan't locate object method "foo" via package "E::F" at/ ? 1 : $@}, 1);

# TODO: we need some tests for the SUPER:: pseudoclass

# failed method call or UNIVERSAL::can() should not autovivify packages
is( $::{"Foo::"} || "none", "none");  # sanity check 1
is( $::{"Foo::"} || "none", "none");  # sanity check 2

is( UNIVERSAL::can("Foo", "boogie") ? "yes":"no", "no" );
is( $::{"Foo::"} || "none", "none");  # still missing?

is( Foo->UNIVERSAL::can("boogie")   ? "yes":"no", "no" );
is( $::{"Foo::"} || "none", "none");  # still missing?

is( Foo->can("boogie")              ? "yes":"no", "no" );
is( $::{"Foo::"} || "none", "none");  # still missing?

is( eval 'Foo->boogie(); 1'         ? "yes":"no", "no" );
is( $::{"Foo::"} || "none", "none");  # still missing?

is(do { eval 'Foo->boogie()';
	  $@ =~ /^\QCan't locate object method "boogie" via package "Foo" (perhaps / ? 1 : $@}, 1);

eval 'sub Foo::boogie { "yes, sir!" }';
is( $::{"Foo::"} ? "ok" : "none", "ok");  # should exist now
is( Foo->boogie(), "yes, sir!");

# TODO: universal.t should test NoSuchPackage->isa()/can()

# This is actually testing parsing of indirect objects and undefined subs
#   print foo("bar") where foo does not exist is not an indirect object.
#   print foo "bar"  where foo does not exist is an indirect object.
eval { sub AUTOLOAD { "ok ", shift, "\n"; } };
ok(1);

# Bug ID 20010902.002
is(
    eval q[
	$x = 'x';
	sub Foo::x : lvalue { $x }
	Foo->$x = 'ok';
    ] || $@, 'ok'
);

# An autoloaded, inherited DESTROY may be invoked differently than normal
# methods, and has been known to give rise to spurious warnings
# eg <200203121600.QAA11064@gizmo.fdgroup.co.uk>

{
    use warnings;
    my $w = '';
    local $SIG{__WARN__} = sub { $w = $_[0] };

    sub AutoDest::Base::AUTOLOAD {}
    @AutoDest::ISA = qw(AutoDest::Base);
    { my $x = bless {}, 'AutoDest'; }
    $w =~ s/\n//g;
    is($w, '');
}

# [ID 20020305.025] PACKAGE::SUPER doesn't work anymore

package main;
our @X;
package Amajor;
sub test {
    push @main::X, 'Amajor', @_;
}
package Bminor;
use base qw(Amajor);
package main;
sub Bminor::test {
    $_[0]->Bminor::SUPER::test('x', 'y');
    push @main::X, 'Bminor', @_;
}
Bminor->test('y', 'z');
is("@X", "Amajor Bminor x y Bminor Bminor y z");

