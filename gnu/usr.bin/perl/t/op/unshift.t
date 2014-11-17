#!./perl

BEGIN {
    require "test.pl";
}

plan(36);

@array = (1, 2, 3);
$aref  = [1, 2, 3];

no warnings 'experimental::autoderef';
{
    no warnings 'syntax';
    $count3 = unshift (@array);
    $count3r = unshift ($aref);
}
is(join(' ',@array), '1 2 3', 'unshift null');
cmp_ok($count3, '==', 3, 'unshift count == 3');
is(join(' ',@$aref), '1 2 3', 'unshift null (ref)');
cmp_ok($count3r, '==', 3, 'unshift count == 3 (ref)');


$count3_2 = unshift (@array, ());
is(join(' ',@array), '1 2 3', 'unshift null empty');
cmp_ok($count3_2, '==', 3, 'unshift count == 3 again');
$count3_2r = unshift ($aref, ());
is(join(' ',@$aref), '1 2 3', 'unshift null empty (ref)');
cmp_ok($count3_2r, '==', 3, 'unshift count == 3 again (ref)');

$count4 = unshift (@array, 0);
is(join(' ',@array), '0 1 2 3', 'unshift singleton list');
cmp_ok($count4, '==', 4, 'unshift count == 4');
$count4r = unshift ($aref, 0);
is(join(' ',@$aref), '0 1 2 3', 'unshift singleton list (ref)');
cmp_ok($count4r, '==', 4, 'unshift count == 4 (ref)');

$count7 = unshift (@array, 3, 2, 1);
is(join(' ',@array), '3 2 1 0 1 2 3', 'unshift list');
cmp_ok($count7, '==', 7, 'unshift count == 7');
$count7r = unshift ($aref, 3, 2, 1);
is(join(' ',@$aref), '3 2 1 0 1 2 3', 'unshift list (ref)');
cmp_ok($count7r, '==', 7, 'unshift count == 7 (ref)');

@list = (5, 4);
$count9 = unshift (@array, @list);
is(join(' ',@array), '5 4 3 2 1 0 1 2 3', 'unshift array');
cmp_ok($count9, '==', 9, 'unshift count == 9');
$count9r = unshift ($aref, @list);
is(join(' ',@$aref), '5 4 3 2 1 0 1 2 3', 'unshift array (ref)');
cmp_ok($count9r, '==', 9, 'unshift count == 9 (ref)');


@list = (7);
@list2 = (6);
$count11 = unshift (@array, @list, @list2);
is(join(' ',@array), '7 6 5 4 3 2 1 0 1 2 3', 'unshift arrays');
cmp_ok($count11, '==', 11, 'unshift count == 11');
$count11r = unshift ($aref, @list, @list2);
is(join(' ',@$aref), '7 6 5 4 3 2 1 0 1 2 3', 'unshift arrays (ref)');
cmp_ok($count11r, '==', 11, 'unshift count == 11 (ref)');

# ignoring counts
@alpha = ('y', 'z');
$alpharef = ['y', 'z'];

{
    no warnings 'syntax';
    unshift (@alpha);
    unshift ($alpharef);
}
is(join(' ',@alpha), 'y z', 'void unshift null');
is(join(' ',@$alpharef), 'y z', 'void unshift null (ref)');

unshift (@alpha, ());
is(join(' ',@alpha), 'y z', 'void unshift null empty');
unshift ($alpharef, ());
is(join(' ',@$alpharef), 'y z', 'void unshift null empty (ref)');

unshift (@alpha, 'x');
is(join(' ',@alpha), 'x y z', 'void unshift singleton list');
unshift ($alpharef, 'x');
is(join(' ',@$alpharef), 'x y z', 'void unshift singleton list (ref)');

unshift (@alpha, 'u', 'v', 'w');
is(join(' ',@alpha), 'u v w x y z', 'void unshift list');
unshift ($alpharef, 'u', 'v', 'w');
is(join(' ',@$alpharef), 'u v w x y z', 'void unshift list (ref)');

@bet = ('s', 't');
unshift (@alpha, @bet);
is(join(' ',@alpha), 's t u v w x y z', 'void unshift array');
unshift ($alpharef, @bet);
is(join(' ',@$alpharef), 's t u v w x y z', 'void unshift array (ref)');

@bet = ('q');
@gimel = ('r');
unshift (@alpha, @bet, @gimel);
is(join(' ',@alpha), 'q r s t u v w x y z', 'void unshift arrays');
unshift ($alpharef, @bet, @gimel);
is(join(' ',@$alpharef), 'q r s t u v w x y z', 'void unshift arrays (ref)');

