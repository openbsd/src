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

{
    class Testcase1 {
        method hello { return "hello, world"; }

        method classname { return __CLASS__; }
    }

    my $obj = Testcase1->new;
    isa_ok($obj, "Testcase1", '$obj');

    is($obj->hello, "hello, world", '$obj->hello');

    is($obj->classname, "Testcase1", '$obj->classname yields __CLASS__');
}

# Classes are still regular packages
{
    class Testcase2 {
        my $ok = "OK";
        sub NotAMethod { return $ok }
    }

    is(Testcase2::NotAMethod(), "OK", 'Class can contain regular subs');
}

# Classes accept full package names
{
    class Testcase3::Foo {
        method hello { return "This" }
    }
    is(Testcase3::Foo->new->hello, "This", 'Class supports fully-qualified package names');
}

# Unit class
{
    class Testcase4::A;
    method m { return "unit-A" }

    class Testcase4::B;
    method m { return "unit-B" }

    package main;
    ok(eq_array([Testcase4::A->new->m, Testcase4::B->new->m], ["unit-A", "unit-B"]),
        'Unit class syntax works');
}

# Class {BLOCK} syntax parses like package
{
    my $result = "";
    eval q{
        $result .= "a(" . __PACKAGE__ . "/" . eval("__PACKAGE__") . ")\n";
        class Testcase5 1.23 {
            $result .= "b(" . __PACKAGE__ . "/" . eval("__PACKAGE__") . ")\n";
        }
        $result .= "c(" . __PACKAGE__ . "/" . eval("__PACKAGE__") . ")\n";
    } or die $@;
    is($result, "a(main/main)\nb(Testcase5/Testcase5)\nc(main/main)\n",
        'class sets __PACKAGE__ correctly');
    is($Testcase5::VERSION, 1.23, 'class NAME VERSION { BLOCK } sets $VERSION');
}

# Unit class syntax parses like package
{
    my $result = "";
    eval q{
        $result .= "a(" . __PACKAGE__ . "/" . eval("__PACKAGE__") . ")\n";
        class Testcase6 4.56;
        $result .= "b(" . __PACKAGE__ . "/" . eval("__PACKAGE__") . ")\n";
        package main;
        $result .= "c(" . __PACKAGE__ . "/" . eval("__PACKAGE__") . ")\n";
    } or die $@;
    is($result, "a(main/main)\nb(Testcase6/Testcase6)\nc(main/main)\n",
        'class sets __PACKAGE__ correctly');
    is($Testcase6::VERSION, 4.56, 'class NAME VERSION; sets $VERSION');
}

done_testing;
