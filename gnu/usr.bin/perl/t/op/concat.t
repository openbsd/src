#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

# This ok() function is specially written to avoid any concatenation.
my $test = 1;
sub ok {
    my($ok, $name) = @_;

    printf "%sok %d - %s\n", ($ok ? "" : "not "), $test, $name;

    printf "# Failed test at line %d\n", (caller)[2] unless $ok;

    $test++;
    return $ok;
}

print "1..19\n";

($a, $b, $c) = qw(foo bar);

ok("$a"     eq "foo",    "verifying assign");
ok("$a$b"   eq "foobar", "basic concatenation");
ok("$c$a$c" eq "foo",    "concatenate undef, fore and aft");

# Okay, so that wasn't very challenging.  Let's go Unicode.

{
    # bug id 20000819.004 

    $_ = $dx = "\x{10f2}";
    s/($dx)/$dx$1/;
    {
        ok($_ eq  "$dx$dx","bug id 20000819.004, back");
    }

    $_ = $dx = "\x{10f2}";
    s/($dx)/$1$dx/;
    {
        ok($_ eq  "$dx$dx","bug id 20000819.004, front");
    }

    $dx = "\x{10f2}";
    $_  = "\x{10f2}\x{10f2}";
    s/($dx)($dx)/$1$2/;
    {
        ok($_ eq  "$dx$dx","bug id 20000819.004, front and back");
    }
}

{
    # bug id 20000901.092
    # test that undef left and right of utf8 results in a valid string

    my $a;
    $a .= "\x{1ff}";
    ok($a eq  "\x{1ff}", "bug id 20000901.092, undef left");
    $a .= undef;
    ok($a eq  "\x{1ff}", "bug id 20000901.092, undef right");
}

{
    # ID 20001020.006

    "x" =~ /(.)/; # unset $2

    # Without the fix this 5.7.0 would croak:
    # Modification of a read-only value attempted at ...
    eval {"$2\x{1234}"};
    ok(!$@, "bug id 20001020.006, left");

    # For symmetry with the above.
    eval {"\x{1234}$2"};
    ok(!$@, "bug id 20001020.006, right");

    *pi = \undef;
    # This bug existed earlier than the $2 bug, but is fixed with the same
    # patch. Without the fix this 5.7.0 would also croak:
    # Modification of a read-only value attempted at ...
    eval{"$pi\x{1234}"};
    ok(!$@, "bug id 20001020.006, constant left");

    # For symmetry with the above.
    eval{"\x{1234}$pi"};
    ok(!$@, "bug id 20001020.006, constant right");
}

sub beq { use bytes; $_[0] eq $_[1]; }

{
    # concat should not upgrade its arguments.
    my($l, $r, $c);

    ($l, $r, $c) = ("\x{101}", "\x{fe}", "\x{101}\x{fe}");
    ok(beq($l.$r, $c), "concat utf8 and byte");
    ok(beq($l, "\x{101}"), "right not changed after concat u+b");
    ok(beq($r, "\x{fe}"), "left not changed after concat u+b");

    ($l, $r, $c) = ("\x{fe}", "\x{101}", "\x{fe}\x{101}");
    ok(beq($l.$r, $c), "concat byte and utf8");
    ok(beq($l, "\x{fe}"), "right not changed after concat b+u");
    ok(beq($r, "\x{101}"), "left not changed after concat b+u");
}

{
    my $a; ($a .= 5) . 6;
    ok($a == 5, '($a .= 5) . 6 - present since 5.000');
}
