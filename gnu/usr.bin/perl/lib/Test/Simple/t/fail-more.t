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
$TB->plan(tests => 17);

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


package main;

require Test::More;
my $Total = 28;
Test::More->import(tests => $Total);

my $tb = Test::More->builder;
$tb->use_numbers(0);

my $Filename = quotemeta $0;

# Preserve the line numbers.
#line 38
ok( 0, 'failing' );
err_ok( <<ERR );
#   Failed test 'failing'
#   in $0 at line 38.
ERR

#line 40
is( "foo", "bar", 'foo is bar?');
is( undef, '',    'undef is empty string?');
is( undef, 0,     'undef is 0?');
is( '',    0,     'empty string is 0?' );
err_ok( <<ERR );
#   Failed test 'foo is bar?'
#   in $0 at line 40.
#          got: 'foo'
#     expected: 'bar'
#   Failed test 'undef is empty string?'
#   in $0 at line 41.
#          got: undef
#     expected: ''
#   Failed test 'undef is 0?'
#   in $0 at line 42.
#          got: undef
#     expected: '0'
#   Failed test 'empty string is 0?'
#   in $0 at line 43.
#          got: ''
#     expected: '0'
ERR

#line 45
isnt("foo", "foo", 'foo isnt foo?' );
isn't("foo", "foo",'foo isn\'t foo?' );
isnt(undef, undef, 'undef isnt undef?');
err_ok( <<ERR );
#   Failed test 'foo isnt foo?'
#   in $0 at line 45.
#     'foo'
#         ne
#     'foo'
#   Failed test 'foo isn\'t foo?'
#   in $0 at line 46.
#     'foo'
#         ne
#     'foo'
#   Failed test 'undef isnt undef?'
#   in $0 at line 47.
#     undef
#         ne
#     undef
ERR

#line 48
like( "foo", '/that/',  'is foo like that' );
unlike( "foo", '/foo/', 'is foo unlike foo' );
err_ok( <<ERR );
#   Failed test 'is foo like that'
#   in $0 at line 48.
#                   'foo'
#     doesn't match '/that/'
#   Failed test 'is foo unlike foo'
#   in $0 at line 49.
#                   'foo'
#           matches '/foo/'
ERR

# Nick Clark found this was a bug.  Fixed in 0.40.
# line 60
like( "bug", '/(%)/',   'regex with % in it' );
err_ok( <<ERR );
#   Failed test 'regex with % in it'
#   in $0 at line 60.
#                   'bug'
#     doesn't match '/(%)/'
ERR

#line 67
fail('fail()');
err_ok( <<ERR );
#   Failed test 'fail()'
#   in $0 at line 67.
ERR

#line 52
can_ok('Mooble::Hooble::Yooble', qw(this that));
can_ok('Mooble::Hooble::Yooble', ());
err_ok( <<ERR );
#   Failed test 'Mooble::Hooble::Yooble->can(...)'
#   in $0 at line 52.
#     Mooble::Hooble::Yooble->can('this') failed
#     Mooble::Hooble::Yooble->can('that') failed
#   Failed test 'Mooble::Hooble::Yooble->can(...)'
#   in $0 at line 53.
#     can_ok() called with no methods
ERR

#line 55
isa_ok(bless([], "Foo"), "Wibble");
isa_ok(42,    "Wibble", "My Wibble");
isa_ok(undef, "Wibble", "Another Wibble");
isa_ok([],    "HASH");
err_ok( <<ERR );
#   Failed test 'The object isa Wibble'
#   in $0 at line 55.
#     The object isn't a 'Wibble' it's a 'Foo'
#   Failed test 'My Wibble isa Wibble'
#   in $0 at line 56.
#     My Wibble isn't a reference
#   Failed test 'Another Wibble isa Wibble'
#   in $0 at line 57.
#     Another Wibble isn't defined
#   Failed test 'The object isa HASH'
#   in $0 at line 58.
#     The object isn't a 'HASH' it's a 'ARRAY'
ERR

#line 68
cmp_ok( 'foo', 'eq', 'bar', 'cmp_ok eq' );
cmp_ok( 42.1,  '==', 23,  , '       ==' );
cmp_ok( 42,    '!=', 42   , '       !=' );
cmp_ok( 1,     '&&', 0    , '       &&' );
err_ok( <<ERR );
#   Failed test 'cmp_ok eq'
#   in $0 at line 68.
#          got: 'foo'
#     expected: 'bar'
#   Failed test '       =='
#   in $0 at line 69.
#          got: 42.1
#     expected: 23
#   Failed test '       !='
#   in $0 at line 70.
#     '42'
#         !=
#     '42'
#   Failed test '       &&'
#   in $0 at line 71.
#     '1'
#         &&
#     '0'
ERR


# line 196
cmp_ok( 42,    'eq', "foo", '       eq with numbers' );
err_ok( <<ERR );
#   Failed test '       eq with numbers'
#   in $0 at line 196.
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
#   in $0 at line 211.
#          got: 42
#     expected: foo
ERR
    My::Test::like $warnings,
     qq[/^Argument "foo" isn't numeric in .* at $Filename line 211\\\.\n\$/];

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
#   in $0 at line 80.
#          got: '$Errno_String'
#     expected: ''
#   Failed test '       eq with numerified errno'
#   in $0 at line 81.
#          got: $Errno_Number
#     expected: -1
ERR

#line 84
use_ok('Hooble::mooble::yooble');

my $more_err_re = <<ERR;
#   Failed test 'use Hooble::mooble::yooble;'
#   in $Filename at line 84\\.
#     Tried to use 'Hooble::mooble::yooble'.
#     Error:  Can't locate Hooble.* in \\\@INC .*
# BEGIN failed--compilation aborted at $Filename line 84.
ERR

My::Test::like($err->read, "/^$more_err_re/");


#line 85
require_ok('ALL::YOUR::BASE::ARE::BELONG::TO::US::wibble');
$more_err_re = <<ERR;
#   Failed test 'require ALL::YOUR::BASE::ARE::BELONG::TO::US::wibble;'
#   in $Filename at line 85\\.
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
not ok - The object isa Wibble
not ok - My Wibble isa Wibble
not ok - Another Wibble isa Wibble
not ok - The object isa HASH
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
