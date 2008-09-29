#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

# Do a basic test on all the tied methods of Tie::Hash::NamedCapture

print "1..13\n";

# PL_curpm->paren_names can be a null pointer. See that this succeeds anyway.
'x' =~ /(.)/;
() = %+;
pass( 'still alive' );

"hlagh" =~ /
    (?<a>.)
    (?<b>.)
    (?<a>.)
    .*
    (?<e>$)
/x;

# FETCH
is($+{a}, "h", "FETCH");
is($+{b}, "l", "FETCH");
is($-{a}[0], "h", "FETCH");
is($-{a}[1], "a", "FETCH");

# STORE
eval { $+{a} = "yon" };
ok(index($@, "read-only") != -1, "STORE");

# DELETE
eval { delete $+{a} };
ok(index($@, "read-only") != -1, "DELETE");

# CLEAR
eval { %+ = () };
ok(index($@, "read-only") != -1, "CLEAR");

# EXISTS
ok(exists $+{e}, "EXISTS");
ok(!exists $+{d}, "EXISTS");

# FIRSTKEY/NEXTKEY
is(join('|', sort keys %+), "a|b|e", "FIRSTKEY/NEXTKEY");

# SCALAR
is(scalar(%+), 3, "SCALAR");
is(scalar(%-), 3, "SCALAR");
