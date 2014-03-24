#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

$|  = 1;

@a = (1..10);

sub j { join(":",@_) }

is( j(splice(@a,@a,0,11,12)), '', 'return value of splice when nothing is removed, only added');
is( j(@a), j(1..12), '... added two elements');

is( j(splice(@a,-1)), "12", 'remove last element, return value');
is( j(@a), j(1..11), '... removed last element');

is( j(splice(@a,0,1)), "1", 'remove first element, return value');
is( j(@a), j(2..11), '... first element removed');

is( j(splice(@a,0,0,0,1)), "", 'emulate shift, return value is empty');
is( j(@a), j(0..11), '... added two elements to beginning of the list');

is( j(splice(@a,5,1,5)), "5", 'remove and replace an element to the end of the list, return value is the element');
is( j(@a), j(0..11), '... list remains the same');

is( j(splice(@a, @a, 0, 12, 13)), "", 'push two elements onto the end of the list, return value is empty');
is( j(@a), j(0..13), '... added two elements to the end of the list');

is( j(splice(@a, -@a, @a, 1, 2, 3)), j(0..13), 'splice the whole list out, add 3 elements, return value is @a');
is( j(@a), j(1..3), '... array only contains new elements');

is( j(splice(@a, 1, -1, 7, 7)), "2", 'replace middle element with two elements, negative offset, return value is the element' );
is( j(@a), j(1,7,7,3), '... array 1,7,7,3');

is( j(splice(@a,-3,-2,2)), j(7), 'replace first 7 with a 2, negative offset, negative length, return value is 7');
is( j(@a), j(1,2,7,3), '... array has 1,2,7,3');

# Bug 20000223.001 - no test for splice(@array).  Destructive test!
is( j(splice(@a)), j(1,2,7,3), 'bare splice empties the array, return value is the array');
is( j(@a),  '', 'array is empty');

# Tests 11 and 12:
# [ID 20010711.005] in Tie::Array, SPLICE ignores context, breaking SHIFT

my $foo;

@a = ('red', 'green', 'blue');
$foo = splice @a, 1, 2;
is( $foo, 'blue', 'remove a single element in scalar context');

@a = ('red', 'green', 'blue');
$foo = shift @a;
is( $foo, 'red', 'do the same with shift');

# Bug [perl #30568] - insertions of deleted elements
@a = (1, 2, 3);
splice( @a, 0, 3, $a[1], $a[0] );
is( j(@a), j(2,1), 'splice and replace with indexes 1, 0');

@a = (1, 2, 3);
splice( @a, 0, 3 ,$a[0], $a[1] );
is( j(@a), j(1,2), 'splice and replace with indexes 0, 1');

@a = (1, 2, 3);
splice( @a, 0, 3 ,$a[2], $a[1], $a[0] );
is( j(@a), j(3,2,1), 'splice and replace with indexes 2, 1, 0');

@a = (1, 2, 3);
splice( @a, 0, 3, $a[0], $a[1], $a[2], $a[0], $a[1], $a[2] );
is( j(@a), j(1,2,3,1,2,3), 'splice and replace with a whole bunch');

@a = (1, 2, 3);
splice( @a, 1, 2, $a[2], $a[1] );
is( j(@a), j(1,3,2), 'swap last two elements');

@a = (1, 2, 3);
splice( @a, 1, 2, $a[1], $a[1] );
is( j(@a), j(1,2,2), 'duplicate middle element on the end');

# splice should invoke get magic

ok( ! Foo->isa('Bar'), 'Foo is not a Bar');

splice @Foo::ISA, 0, 0, 'Bar';
ok( !oo->isa('Bar'), 'splice @ISA and make Foo a Bar');

# Test undef first arg
eval { splice( $new_arrayref, 0, 0, 1, 2, 3 ) };
like($@, qr/Not an ARRAY/, 'undefined first argument to splice');

done_testing;
