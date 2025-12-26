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

class Base {
    method g() { "Base" }
    ADJUST {
        ::fail("original Base ADJUST block should not be called");
    }
}

class Base2 {
    method g() { "Base2" }
}

BEGIN {
    our $saw_end;
    eval <<'CLASS';
class MyTest :isa(Base) {
    field $x = "First";
    field $w :reader;
    ADJUST {
        ::fail("ADJUST from failed class definition called");
    }
    method f () { $x }
    method h() { }
    method z() { }
    # make sure some error above doesn't invalidate the test, this
    BEGIN { ++$saw_end; }
# no }
CLASS
    ok($saw_end, "saw the end of the incomplete class definition");
}

class MyTest :isa(Base2) {
    field $y = "Second";
    method f() { $y }
    ADJUST {
        ::pass("saw adjust in replacement class definition");
    }
}

my $z = new_ok("MyTest");
ok(!$z->can("h"), "h() should no longer be present");
isa_ok($z, "Base2", "check base class");
is($z->g(), "Base2", "Base class correct via g");
is($z->f(), "Second", "f() value");
ok(!$z->can("w"), 'accessor for $w removed');

done_testing();

