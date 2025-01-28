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

done_testing;
