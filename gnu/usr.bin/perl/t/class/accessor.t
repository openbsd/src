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

# reader accessors
{
    class Testcase1 {
        field $s :reader = "the scalar";

        field @a :reader = qw( the array );

        # Present-but-empty parens counts as default
        field %h :reader() = qw( the hash );
    }

    my $o = Testcase1->new;
    is($o->s, "the scalar", '$o->s accessor');
    ok(eq_array([$o->a], [qw( the array )]), '$o->a accessor');
    ok(eq_hash({$o->h}, {qw( the hash )}), '$o->h accessor');

    is(scalar $o->a, 2, '$o->a accessor in scalar context');
    is(scalar $o->h, 1, '$o->h accessor in scalar context');

    # Read accessor does not permit arguments
    ok(!eval { $o->s("value") },
        'Reader accessor fails with argument');
    like($@, qr/^Too many arguments for subroutine \'Testcase1::s\' \(got 1; expected 0\) at /,
        'Failure from argument to accessor');
}

# writer accessors on scalars
{
    class Testcase2 {
        field $s :reader :writer = "initial";
    }

    my $o = Testcase2->new;
    is($o->s, "initial", '$o->s accessor before modification');
    is($o->set_s("new-value"), $o, '$o->set_s accessor returns instance');
    is($o->s, "new-value", '$o->s accessor after modification');

    # Write accessor wants exactly one argument
    ok(!eval { $o->set_s() },
        'Reader accessor fails with no argument');
    like($@, qr/^Too few arguments for subroutine \'Testcase2::set_s\' \(got 0; expected 1\) at /,
        'Failure from argument to accessor');
    ok(!eval { $o->set_s(1, 2) },
        'Reader accessor fails with 2 arguments');
    like($@, qr/^Too many arguments for subroutine \'Testcase2::set_s\' \(got 2; expected 1\) at /,
        'Failure from argument to accessor');
}

# Alternative names
{
    class Testcase3 {
        field $f :reader(get_f) :writer(write_f) = "value";
    }

    is(Testcase3->new->get_f, "value",
        'read accessor with altered name');
    ok(Testcase3->new->write_f("new"),
        'write accessor with altered name');

    ok(!eval { Testcase3->new->f },
       'Accessor with altered name does not also generate original name');
    like($@, qr/^Can't locate object method "f" via package "Testcase3" at /,
       'Failure from lack of original name accessor');
}

done_testing;
