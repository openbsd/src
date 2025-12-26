#!./perl

BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
    set_up_inc('../lib');
    require Config;
}

use v5.36;
use feature 'class';
no warnings 'experimental::class';

# $self in method
{
    class Testcase1 {
        method retself { return $self }
    }

    my $obj = Testcase1->new;
    is($obj->retself, $obj, '$self inside method');
}

# methods have signatures; signatures do not capture $self
{
    # Turn off the 'signatures' feature to prove that 'method' is always
    # signatured even without it
    no feature 'signatures';

    class Testcase2 {
        method retfirst ( $x = 123 ) { return $x; }
    }

    my $obj = Testcase2->new;
    is($obj->retfirst,      123, 'method signature params work');
    is($obj->retfirst(456), 456, 'method signature params skip $self');
}

# methods can still capture regular package lexicals
{
    class Testcase3 {
        my $count;
        method inc { return $count++ }
    }

    my $obj1 = Testcase3->new;
    $obj1->inc;

    is($obj1->inc, 1, '$obj1->inc sees 1');

    my $obj2 = Testcase3->new;
    is($obj2->inc, 2, '$obj2->inc sees 2');
}

# $self is shifted from @_
{
    class Testcase4 {
        method args { return @_ }
    }

    my $obj = Testcase4->new;
    ok(eq_array([$obj->args("a", "b")], ["a", "b"]), '$self is shifted from @_');
}

# anon methods
{
    class Testcase5 {
        method anonmeth {
            return method {
                return "Result";
            }
        }
    }

    my $obj = Testcase5->new;
    my $mref = $obj->anonmeth;

    is($obj->$mref, "Result", 'anon method can be invoked');
}

# methods can be forward declared without a body
{
    class Testcase6 {
        method forwarded;

        method forwarded { return "OK" }
    }

    is(Testcase6->new->forwarded, "OK", 'forward-declared method works');
}

# methods can be declared lexically
{
    class Testcase7 {
        my method priv {
            return "private-result";
        }

        method m { return priv($self); }
    }

    is(Testcase7->new->m, "private-result", 'lexical method can be declared and called');
    ok(!Testcase7->can("priv"), 'lexical method does not appear in the symbol table');
}

# ->& operator can invoke methods with lexical scope
{
    class Testcase8 {
        field $f = "private-result";

        my method priv {
            return $f;
        }

        method notpriv {
            return "pkg-result";
        }

        method lexm_paren { return $self->&priv(); }
        method lexm_plain { return $self->&priv; }

        method pkgm       { return $self->&notpriv; }
    }

    is(Testcase8->new->lexm_paren, "private-result", 'lexical method can be invoked with ->&m()');
    is(Testcase8->new->lexm_plain, "private-result", 'lexical method can be invoked with ->&m');
    is(Testcase8->new->pkgm,       "pkg-result",     'package method can be invoked with ->&m');

    class Testcase8Derived :isa(Testcase8) {
        method notpriv {
            return "different result";
        }
    }

    is(Testcase8Derived->new->pkgm, "pkg-result",
        '->&m operator does not follow inheritance');
}

# lexical methods with signatures work correctly (GH#23030)
{
    class Testcase9 {
        field $x = 123;

        my method priv ( $y ) {
            return "X is $x and Y is $y for $self";
        }

        method test {
            $self->&priv(456);
        }
    }

    like(Testcase9->new->test, qr/^X is 123 and Y is 456 for Testcase9=OBJECT\(0x.*\)$/,
        'lexical method with signature counts $self correctly');
}

done_testing;
