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

# We can't test fields in isolation without having at least one method to
# use them from. We'll try to keep most of the heavy testing of method
# abilities to t/class/method.t

# field in method
{
    class Testcase1 {
        field $f;
        method incr { return ++$f; }
    }

    my $obj = Testcase1->new;
    $obj->incr;
    is($obj->incr, 2, 'Field $f incremented twice');

    my $obj2 = Testcase1->new;
    is($obj2->incr, 1, 'Fields are distinct between instances');
}

# fields are distinct
{
    class Testcase2 {
        field $x;
        field $y;

        method setpos { $x = $_[0]; $y = $_[1] }
        method x      { return $x; }
        method y      { return $y; }
    }

    my $obj = Testcase2->new;
    $obj->setpos(10, 20);
    is($obj->x, 10, '$pos->x');
    is($obj->y, 20, '$pos->y');
}

# fields of all variable types
{
    class Testcase3 {
        field $s;
        field @a;
        field %h;

        method setup {
            $s = "scalar";
            @a = ( "array" );
            %h = ( key => "hash" );
            return $self; # test chaining
        }
        method test {
            ::is($s,      "scalar", 'scalar storage');
            ::is($a[0],   "array",  'array storage');
            ::is($h{key}, "hash",   'hash storage');
        }
    }

    Testcase3->new->setup->test;
}

# fields can be captured by anon subs
{
    class Testcase4 {
        field $count;

        method make_incrsub {
            return sub { $count++ };
        }

        method count { return $count }
    }

    my $obj = Testcase4->new;
    my $incr = $obj->make_incrsub;

    $incr->();
    $incr->();
    $incr->();

    is($obj->count, 3, '$obj->count after invoking closure x 3');
}

# fields can be captured by anon methods
{
    class Testcase5 {
        field $count;

        method make_incrmeth {
            return method { $count++ };
        }

        method count { return $count }
    }

    my $obj = Testcase5->new;
    my $incr = $obj->make_incrmeth;

    $obj->$incr;
    $obj->$incr;
    $obj->$incr;

    is($obj->count, 3, '$obj->count after invoking method-closure x 3');
}

# fields of multiple unit classes are distinct
{
    class Testcase6::A;
    field $x = "A";
    method m { return "unit-$x" }

    class Testcase6::B;
    field $x = "B";
    method m { return "unit-$x" }

    package main;
    ok(eq_array([Testcase6::A->new->m, Testcase6::B->new->m], ["unit-A", "unit-B"]),
        'Fields of multiple unit classes remain distinct');
}

# fields can be initialised with constant expressions
{
    class Testcase7 {
        field $scalar = 123;
        method scalar { return $scalar; }

        field @array = (4, 5, 6);
        method array { return @array; }

        field %hash  = (7 => 89);
        method hash { return %hash; }
    }

    my $obj = Testcase7->new;

    is($obj->scalar, 123, 'Scalar field can be constant initialised');

    ok(eq_array([$obj->array], [4, 5, 6]), 'Array field can be constant initialised');

    ok(eq_hash({$obj->hash}, {7 => 89}), 'Hash field can be constant initialised');
}

# field initialiser expressions are evaluated within the constructor of each
# instance
{
    my $next_x = 1;
    my @items;
    my %mappings;

    class Testcase8 {
        field $x = $next_x++;
        method x { return $x; }

        field @y = ("more", @items);
        method y { return @y; }

        field %z = (first => "value", %mappings);
        method z { return %z; }
    }

    is($next_x, 1, '$next_x before any objects');

    @items = ("values");
    $mappings{second} = "here";

    my $obj1 = Testcase8->new;
    my $obj2 = Testcase8->new;

    is($obj1->x, 1, 'Object 1 has x == 1');
    is($obj2->x, 2, 'Object 2 has x == 2');

    is($next_x, 3, '$next_x after constructing two');

    ok(eq_array([$obj1->y], ["more", "values"]),
        'Object 1 has correct array field');
    ok(eq_hash({$obj1->z}, {first => "value", second => "here"}),
        'Object 1 has correct hash field');
}

# fields are visible during initialiser expressions of later fields
{
    class Testcase9 {
        field $one   = 1;
        field $two   = $one + 1;
        field $three = $two + 1;

        field @four = $one;
        field @five = (@four, $two, $three);
        field @six  = grep { $_ > 1 } @five;

        method three { return $three; }

        method six { return @six; }
    }

    my $obj = Testcase9->new;
    is($obj->three, 3, 'Scalar fields initialised from earlier fields');
    ok(eq_array([$obj->six], [2, 3]), 'Array fields initialised from earlier fields');
}

# fields can take :param attributes to consume constructor parameters
{
    my $next_gamma = 4;

    class Testcase10 {
        field $alpha :param        = undef;
        field $beta  :param        = 123;
        field $gamma :param(delta) = $next_gamma++;

        method values { return ($alpha, $beta, $gamma); }
    }

    my $obj = Testcase10->new(
        alpha => "A",
        beta  => "B",
        delta => "G",
    );
    ok(eq_array([$obj->values], [qw(A B G)]),
        'Field initialised by :params');
    is($next_gamma, 4, 'Defaulting expression not evaluated for passed value');

    $obj = Testcase10->new();
    ok(eq_array([$obj->values], [undef, 123, 4]),
        'Field initialised by defaulting expressions');
    is($next_gamma, 5, 'Defaulting expression evaluated for missing value');
}

# fields can be made non-optional
{
    class Testcase11 {
        field $x :param;
        field $y :param;
    }

    Testcase11->new(x => 1, y => 1);

    ok(!eval { Testcase11->new(x => 2) },
        'Constructor fails without y');
    like($@, qr/^Required parameter 'y' is missing for "Testcase11" constructor at /,
        'Failure from missing y argument');
}

# field assignment expressions on :param can use //= and ||=
{
    class Testcase12 {
        field $if_exists  :param(e)   = "DEF";
        field $if_defined :param(d) //= "DEF";
        field $if_true    :param(t) ||= "DEF";

        method values { return ($if_exists, $if_defined, $if_true); }
    }

    ok(eq_array(
        [Testcase12->new(e => "yes", d => "yes", t => "yes")->values],
        ["yes", "yes", "yes"]),
        'Values for "yes"');

    ok(eq_array(
        [Testcase12->new(e => 0, d => 0, t => 0)->values],
        [0, 0, "DEF"]),
        'Values for 0');

    ok(eq_array(
        [Testcase12->new(e => undef, d => undef, t => undef)->values],
        [undef, "DEF", "DEF"]),
        'Values for undef');

    ok(eq_array(
        [Testcase12->new()->values],
        ["DEF", "DEF", "DEF"]),
        'Values for missing');
}

# field initialiser expressions permit `goto` in do {} blocks
{
    class Testcase13 {
        field $forwards = do { goto HERE; HERE: 1 };
        field $backwards = do { my $x; HERE: ; goto HERE if !$x++; 2 };

        method values { return ($forwards, $backwards) }
    }

    ok(eq_array(
        [Testcase13->new->values],
        [1, 2],
        'Values for goto inside do {} blocks in field initialisers'));
}

# field initialiser expressions permit a __CLASS__
{
    class Testcase14 {
        field $classname = __CLASS__;

        method classname { return $classname }
    }

    is(Testcase14->new->classname, "Testcase14", '__CLASS__ in field initialisers');
}

done_testing;
