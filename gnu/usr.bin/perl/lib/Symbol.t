#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Test::More tests => 26;

BEGIN { $_ = 'foo'; }  # because Symbol used to clobber $_

use Symbol;

ok( $_ eq 'foo', 'check $_ clobbering' );


# First test gensym()
$sym1 = gensym;
ok( ref($sym1) eq 'GLOB', 'gensym() returns a GLOB' );

$sym2 = gensym;

ok( $sym1 ne $sym2, 'gensym() returns a different GLOB' );

ungensym $sym1;

$sym1 = $sym2 = undef;

# Test geniosym()

use Symbol qw(geniosym);

$sym1 = geniosym;
like( $sym1, qr/=IO\(/, 'got an IO ref' );

$FOO = 'Eymascalar';
*FOO = $sym1;

is( $sym1, *FOO{IO}, 'assigns into glob OK' );

is( $FOO, 'Eymascalar', 'leaves scalar alone' );

{
    local $^W=1;		# 5.005 compat.
    my $warn;
    local $SIG{__WARN__} = sub { $warn .= "@_" };
    readline FOO;
    like( $warn, qr/unopened filehandle/, 'warns like an unopened filehandle' );
}

# Test qualify()
package foo;

use Symbol qw(qualify);  # must import into this package too

::ok( qualify("x") eq "foo::x",		'qualify() with a simple identifier' );
::ok( qualify("x", "FOO") eq "FOO::x",	'qualify() with a package' );
::ok( qualify("BAR::x") eq "BAR::x",
    'qualify() with a qualified identifier' );
::ok( qualify("STDOUT") eq "main::STDOUT",
    'qualify() with a reserved identifier' );
::ok( qualify("ARGV", "FOO") eq "main::ARGV",
    'qualify() with a reserved identifier and a package' );
::ok( qualify("_foo") eq "foo::_foo",
    'qualify() with an identifier starting with a _' );
::ok( qualify("^FOO") eq "main::\cFOO",
    'qualify() with an identifier starting with a ^' );

# tests for delete_package
package main;
$Transient::variable = 42;
ok( exists $::{'Transient::'}, 'transient stash exists' );
ok( defined $Transient::{variable}, 'transient variable in stash' );
Symbol::delete_package('Transient');
ok( !exists $Transient::{variable}, 'transient variable no longer in stash' );
is( scalar(keys %Transient::), 0, 'transient stash is empty' );
ok( !exists $::{'Transient::'}, 'no transient stash' );

$Foo::variable = 43;
ok( exists $::{'Foo::'}, 'second transient stash exists' );
ok( defined $Foo::{variable}, 'second transient variable in stash' );
Symbol::delete_package('::Foo');
is( scalar(keys %Foo::), 0, 'second transient stash is empty' );
ok( !exists $::{'Foo::'}, 'no second transient stash' );

$Bar::variable = 44;
ok( exists $::{'Bar::'}, 'third transient stash exists' );
ok( defined $Bar::{variable}, 'third transient variable in stash' );
ok( ! defined(Symbol::delete_package('Bar::Bar::')),
    'delete_package() returns undef due to undefined leaf');
