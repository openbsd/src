#!perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;

require Test::Simple::Catch;
my($out, $err) = Test::Simple::Catch::caught();
local $ENV{HARNESS_ACTIVE} = 0;


# Can't use Test.pm, that's a 5.005 thing.
package My::Test;

# This has to be a require or else the END block below runs before
# Test::Builder's own and the ending diagnostics don't come out right.
require Test::Builder;
my $TB = Test::Builder->create;
$TB->plan(tests => 23);

sub like ($$;$) {
    $TB->like(@_);
}

sub is ($$;$) {
    $TB->is_eq(@_);
}

sub main::err_ok ($) {
    my($expect) = @_;
    my $got = $err->read;

    return $TB->is_eq( $got, $expect );
}

sub main::err_like ($) {
    my($expect) = @_;
    my $got = $err->read;

    return $TB->like( $got, qr/$expect/ );
}


package main;

require Test::More;
my $Total = 36;
Test::More->import(tests => $Total);

# This should all work in the presence of a __DIE__ handler.
local $SIG{__DIE__} = sub { $TB->ok(0, "DIE handler called: ".join "", @_); };


my $tb = Test::More->builder;
$tb->use_numbers(0);

my $Filename = quotemeta $0;

# Preserve the line numbers.
#line 38
ok( 0, 'failing' );
err_ok( <<ERR );
#   Failed test 'failing'
#   at $0 line 38.
ERR

#line 40
is( "foo", "bar", 'foo is bar?');
is( undef, '',    'undef is empty string?');
is( undef, 0,     'undef is 0?');
is( '',    0,     'empty string is 0?' );
err_ok( <<ERR );
#   Failed test 'foo is bar?'
#   at $0 line 40.
#          got: 'foo'
#     expected: 'bar'
#   Failed test 'undef is empty string?'
#   at $0 line 41.
#          got: undef
#     expected: ''
#   Failed test 'undef is 0?'
#   at $0 line 42.
#          got: undef
#     expected: '0'
#   Failed test 'empty string is 0?'
#   at $0 line 43.
#          got: ''
#     expected: '0'
ERR

#line 45
isnt("foo", "foo", 'foo isnt foo?' );
isn't("foo", "foo",'foo isn\'t foo?' );
isnt(undef, undef, 'undef isnt undef?');
err_ok( <<ERR );
#   Failed test 'foo isnt foo?'
#   at $0 line 45.
#          got: 'foo'
#     expected: anything else
#   Failed test 'foo isn\'t foo?'
#   at $0 line 46.
#          got: 'foo'
#     expected: anything else
#   Failed test 'undef isnt undef?'
#   at $0 line 47.
#          got: undef
#     expected: anything else
ERR

#line 48
like( "foo", '/that/',  'is foo like that' );
unlike( "foo", '/foo/', 'is foo unlike foo' );
err_ok( <<ERR );
#   Failed test 'is foo like that'
#   at $0 line 48.
#                   'foo'
#     doesn't match '/that/'
#   Failed test 'is foo unlike foo'
#   at $0 line 49.
#                   'foo'
#           matches '/foo/'
ERR

# Nick Clark found this was a bug.  Fixed in 0.40.
# line 60
like( "bug", '/(%)/',   'regex with % in it' );
err_ok( <<ERR );
#   Failed test 'regex with % in it'
#   at $0 line 60.
#                   'bug'
#     doesn't match '/(%)/'
ERR

#line 67
fail('fail()');
err_ok( <<ERR );
#   Failed test 'fail()'
#   at $0 line 67.
ERR

#line 52
can_ok('Mooble::Hooble::Yooble', qw(this that));
can_ok('Mooble::Hooble::Yooble', ());
can_ok(undef, undef);
can_ok([], "foo");
err_ok( <<ERR );
#   Failed test 'Mooble::Hooble::Yooble->can(...)'
#   at $0 line 52.
#     Mooble::Hooble::Yooble->can('this') failed
#     Mooble::Hooble::Yooble->can('that') failed
#   Failed test 'Mooble::Hooble::Yooble->can(...)'
#   at $0 line 53.
#     can_ok() called with no methods
#   Failed test '->can(...)'
#   at $0 line 54.
#     can_ok() called with empty class or reference
#   Failed test 'ARRAY->can('foo')'
#   at $0 line 55.
#     ARRAY->can('foo') failed
ERR

#line 55
isa_ok(bless([], "Foo"), "Wibble");
isa_ok(42,    "Wibble", "My Wibble");
isa_ok(undef, "Wibble", "Another Wibble");
isa_ok([],    "HASH");
err_ok( <<ERR );
#   Failed test 'The object isa Wibble'
#   at $0 line 55.
#     The object isn't a 'Wibble' it's a 'Foo'
#   Failed test 'My Wibble isa Wibble'
#   at $0 line 56.
#     My Wibble isn't a reference
#   Failed test 'Another Wibble isa Wibble'
#   at $0 line 57.
#     Another Wibble isn't defined
#   Failed test 'The object isa HASH'
#   at $0 line 58.
#     The object isn't a 'HASH' it's a 'ARRAY'
ERR


#line 188
new_ok(undef);
err_like( <<ERR );
#   Failed test 'new\\(\\) died'
#   at $Filename line 188.
#     Error was:  Can't call method "new" on an undefined value at .*
ERR

#line 211
new_ok( "Does::Not::Exist" );
err_like( <<ERR );
#   Failed test 'new\\(\\) died'
#   at $Filename line 211.
#     Error was:  Can't locate object method "new" via package "Does::Not::Exist" .*
ERR

{ package Foo; sub new { } }
{ package Bar; sub new { {} } }
{ package Baz; sub new { bless {}, "Wibble" } }

#line 219
new_ok( "Foo" );
err_ok( <<ERR );
#   Failed test 'The object isa Foo'
#   at $0 line 219.
#     The object isn't defined
ERR

# line 231
new_ok( "Bar" );
err_ok( <<ERR );
#   Failed test 'The object isa Bar'
#   at $0 line 231.
#     The object isn't a 'Bar' it's a 'HASH'
ERR

#line 239
new_ok( "Baz" );
err_ok( <<ERR );
#   Failed test 'The object isa Baz'
#   at $0 line 239.
#     The object isn't a 'Baz' it's a 'Wibble'
ERR

#line 247
new_ok( "Baz", [], "no args" );
err_ok( <<ERR );
#   Failed test 'no args isa Baz'
#   at $0 line 247.
#     no args isn't a 'Baz' it's a 'Wibble'
ERR


#line 68
cmp_ok( 'foo', 'eq', 'bar', 'cmp_ok eq' );
cmp_ok( 42.1,  '==', 23,  , '       ==' );
cmp_ok( 42,    '!=', 42   , '       !=' );
cmp_ok( 1,     '&&', 0    , '       &&' );
err_ok( <<ERR );
#   Failed test 'cmp_ok eq'
#   at $0 line 68.
#          got: 'foo'
#     expected: 'bar'
#   Failed test '       =='
#   at $0 line 69.
#          got: 42.1
#     expected: 23
#   Failed test '       !='
#   at $0 line 70.
#          got: 42
#     expected: anything else
#   Failed test '       &&'
#   at $0 line 71.
#     '1'
#         &&
#     '0'
ERR


# line 196
cmp_ok( 42,    'eq', "foo", '       eq with numbers' );
err_ok( <<ERR );
#   Failed test '       eq with numbers'
#   at $0 line 196.
#          got: '42'
#     expected: 'foo'
ERR


{
    my $warnings;
    local $SIG{__WARN__} = sub { $warnings .= join '', @_ };

# line 211
    cmp_ok( 42,    '==', "foo", '       == with strings' );
    err_ok( <<ERR );
#   Failed test '       == with strings'
#   at $0 line 211.
#          got: 42
#     expected: foo
ERR
    My::Test::like $warnings,
     qr/^Argument "foo" isn't numeric in .* at cmp_ok \[from $Filename line 211\] line 1\.\n$/;

}


# generate a $!, it changes its value by context.
-e "wibblehibble";
my $Errno_Number = $!+0;
my $Errno_String = $!.'';
#line 80
cmp_ok( $!,    'eq', '',    '       eq with stringified errno' );
cmp_ok( $!,    '==', -1,    '       eq with numerified errno' );
err_ok( <<ERR );
#   Failed test '       eq with stringified errno'
#   at $0 line 80.
#          got: '$Errno_String'
#     expected: ''
#   Failed test '       eq with numerified errno'
#   at $0 line 81.
#          got: $Errno_Number
#     expected: -1
ERR

#line 84
use_ok('Hooble::mooble::yooble');

my $more_err_re = <<ERR;
#   Failed test 'use Hooble::mooble::yooble;'
#   at $Filename line 84\\.
#     Tried to use 'Hooble::mooble::yooble'.
#     Error:  Can't locate Hooble.* in \\\@INC .*
ERR

My::Test::like($err->read, "/^$more_err_re/");


#line 85
require_ok('ALL::YOUR::BASE::ARE::BELONG::TO::US::wibble');
$more_err_re = <<ERR;
#   Failed test 'require ALL::YOUR::BASE::ARE::BELONG::TO::US::wibble;'
#   at $Filename line 85\\.
#     Tried to require 'ALL::YOUR::BASE::ARE::BELONG::TO::US::wibble'.
#     Error:  Can't locate ALL.* in \\\@INC .*
ERR

My::Test::like($err->read, "/^$more_err_re/");


#line 88
END {
    $TB->is_eq($$out, <<OUT, 'failing output');
1..$Total
not ok - failing
not ok - foo is bar?
not ok - undef is empty string?
not ok - undef is 0?
not ok - empty string is 0?
not ok - foo isnt foo?
not ok - foo isn't foo?
not ok - undef isnt undef?
not ok - is foo like that
not ok - is foo unlike foo
not ok - regex with % in it
not ok - fail()
not ok - Mooble::Hooble::Yooble->can(...)
not ok - Mooble::Hooble::Yooble->can(...)
not ok - ->can(...)
not ok - ARRAY->can('foo')
not ok - The object isa Wibble
not ok - My Wibble isa Wibble
not ok - Another Wibble isa Wibble
not ok - The object isa HASH
not ok - new() died
not ok - new() died
not ok - The object isa Foo
not ok - The object isa Bar
not ok - The object isa Baz
not ok - no args isa Baz
not ok - cmp_ok eq
not ok -        ==
not ok -        !=
not ok -        &&
not ok -        eq with numbers
not ok -        == with strings
not ok -        eq with stringified errno
not ok -        eq with numerified errno
not ok - use Hooble::mooble::yooble;
not ok - require ALL::YOUR::BASE::ARE::BELONG::TO::US::wibble;
OUT

err_ok( <<ERR );
# Looks like you failed $Total tests of $Total.
ERR

    exit(0);
}
