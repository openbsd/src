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


# Can't use Test.pm, that's a 5.005 thing.
package My::Test;

print "1..2\n";

my $test_num = 1;
# Utility testing functions.
sub ok ($;$) {
    my($test, $name) = @_;
    my $ok = '';
    $ok .= "not " unless $test;
    $ok .= "ok $test_num";
    $ok .= " - $name" if defined $name;
    $ok .= "\n";
    print $ok;
    $test_num++;

    return $test;
}


package main;

require Test::More;
my $Total = 28;
Test::More->import(tests => $Total);

# Preserve the line numbers.
#line 38
ok( 0, 'failing' );

#line 40
is( "foo", "bar", 'foo is bar?');
is( undef, '',    'undef is empty string?');
is( undef, 0,     'undef is 0?');
is( '',    0,     'empty string is 0?' );

isnt("foo", "foo", 'foo isnt foo?' );
isn't("foo", "foo",'foo isn\'t foo?' );

like( "foo", '/that/',  'is foo like that' );
unlike( "foo", '/foo/', 'is foo unlike foo' );

# Nick Clark found this was a bug.  Fixed in 0.40.
like( "bug", '/(%)/',   'regex with % in it' );

fail('fail()');

#line 52
can_ok('Mooble::Hooble::Yooble', qw(this that));
can_ok('Mooble::Hooble::Yooble', ());

isa_ok(bless([], "Foo"), "Wibble");
isa_ok(42,    "Wibble", "My Wibble");
isa_ok(undef, "Wibble", "Another Wibble");
isa_ok([],    "HASH");

#line 68
cmp_ok( 'foo', 'eq', 'bar', 'cmp_ok eq' );
cmp_ok( 42.1,  '==', 23,  , '       ==' );
cmp_ok( 42,    '!=', 42   , '       !=' );
cmp_ok( 1,     '&&', 0    , '       &&' );
cmp_ok( 42,    '==', "foo", '       == with strings' );
cmp_ok( 42,    'eq', "foo", '       eq with numbers' );
cmp_ok( undef, 'eq', 'foo', '       eq with undef' );

# generate a $!, it changes its value by context.
-e "wibblehibble";
my $Errno_Number = $!+0;
my $Errno_String = $!.'';
cmp_ok( $!,    'eq', '',    '       eq with stringified errno' );
cmp_ok( $!,    '==', -1,    '       eq with numerified errno' );

#line 84
use_ok('Hooble::mooble::yooble');
require_ok('ALL::YOUR::BASE::ARE::BELONG::TO::US::wibble');

#line 88
END {
    My::Test::ok($$out eq <<OUT, 'failing output');
1..$Total
not ok 1 - failing
not ok 2 - foo is bar?
not ok 3 - undef is empty string?
not ok 4 - undef is 0?
not ok 5 - empty string is 0?
not ok 6 - foo isnt foo?
not ok 7 - foo isn't foo?
not ok 8 - is foo like that
not ok 9 - is foo unlike foo
not ok 10 - regex with % in it
not ok 11 - fail()
not ok 12 - Mooble::Hooble::Yooble->can(...)
not ok 13 - Mooble::Hooble::Yooble->can(...)
not ok 14 - The object isa Wibble
not ok 15 - My Wibble isa Wibble
not ok 16 - Another Wibble isa Wibble
not ok 17 - The object isa HASH
not ok 18 - cmp_ok eq
not ok 19 -        ==
not ok 20 -        !=
not ok 21 -        &&
not ok 22 -        == with strings
not ok 23 -        eq with numbers
not ok 24 -        eq with undef
not ok 25 -        eq with stringified errno
not ok 26 -        eq with numerified errno
not ok 27 - use Hooble::mooble::yooble;
not ok 28 - require ALL::YOUR::BASE::ARE::BELONG::TO::US::wibble;
OUT

    my $err_re = <<ERR;
#     Failed test ($0 at line 38)
#     Failed test ($0 at line 40)
#          got: 'foo'
#     expected: 'bar'
#     Failed test ($0 at line 41)
#          got: undef
#     expected: ''
#     Failed test ($0 at line 42)
#          got: undef
#     expected: '0'
#     Failed test ($0 at line 43)
#          got: ''
#     expected: '0'
#     Failed test ($0 at line 45)
#     'foo'
#         ne
#     'foo'
#     Failed test ($0 at line 46)
#     'foo'
#         ne
#     'foo'
#     Failed test ($0 at line 48)
#                   'foo'
#     doesn't match '/that/'
#     Failed test ($0 at line 49)
#                   'foo'
#           matches '/foo/'
#     Failed test ($0 at line 52)
#                   'bug'
#     doesn't match '/(%)/'
#     Failed test ($0 at line 54)
#     Failed test ($0 at line 52)
#     Mooble::Hooble::Yooble->can('this') failed
#     Mooble::Hooble::Yooble->can('that') failed
#     Failed test ($0 at line 53)
#     can_ok() called with no methods
#     Failed test ($0 at line 55)
#     The object isn't a 'Wibble' it's a 'Foo'
#     Failed test ($0 at line 56)
#     My Wibble isn't a reference
#     Failed test ($0 at line 57)
#     Another Wibble isn't defined
#     Failed test ($0 at line 58)
#     The object isn't a 'HASH' it's a 'ARRAY'
#     Failed test ($0 at line 68)
#          got: 'foo'
#     expected: 'bar'
#     Failed test ($0 at line 69)
#          got: 42.1
#     expected: 23
#     Failed test ($0 at line 70)
#     '42'
#         !=
#     '42'
#     Failed test ($0 at line 71)
#     '1'
#         &&
#     '0'
#     Failed test ($0 at line 72)
#          got: 42
#     expected: 0
#     Failed test ($0 at line 73)
#          got: '42'
#     expected: 'foo'
#     Failed test ($0 at line 74)
#          got: undef
#     expected: 'foo'
#     Failed test ($0 at line 80)
#          got: '$Errno_String'
#     expected: ''
#     Failed test ($0 at line 81)
#          got: $Errno_Number
#     expected: -1
ERR

   my $filename = quotemeta $0;
   my $more_err_re = <<ERR;
#     Failed test \\($filename at line 84\\)
#     Tried to use 'Hooble::mooble::yooble'.
#     Error:  Can't locate Hooble.* in \\\@INC .*
#     Failed test \\($filename at line 85\\)
#     Tried to require 'ALL::YOUR::BASE::ARE::BELONG::TO::US::wibble'.
#     Error:  Can't locate ALL.* in \\\@INC .*
# Looks like you failed $Total tests of $Total.
ERR

    unless( My::Test::ok($$err =~ /^\Q$err_re\E$more_err_re$/, 
                         'failing errors') ) {
        print $$err;
    }

    exit(0);
}
