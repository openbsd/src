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
    class Testcase1A {
        field $inita = "base";
        method inita { return $inita; }
        field $adja;
        ADJUST { $adja = "base class" }
        method adja { return $adja; }

        method classname { return __CLASS__; }
    }

    class Testcase1B :isa(Testcase1A) {
        field $initb = "derived";
        method initb { return $initb; }
        field $adjb;
        ADJUST { $adjb = "derived class" }
        method adjb { return $adjb; }
    }

    my $obj = Testcase1B->new;
    ok($obj isa Testcase1B, 'Object is its own class');
    ok($obj isa Testcase1A, 'Object is also its base class');

    ok(eq_array(\@Testcase1B::ISA, ["Testcase1A"]), '@Testcase1B::ISA is set correctly');

    is($obj->initb, "derived",       'Object has derived class initialised field');
    is($obj->adjb,  "derived class", 'Object has derived class ADJUSTed field');

    can_ok($obj, "inita");
    is($obj->inita, "base",      'Object has base class initialised field');
    can_ok($obj, "adja");
    is($obj->adja, "base class", 'Object has base class ADJUSTed field');

    is($obj->classname, "Testcase1B", '__CLASS__ yields runtime instance class name');

    class Testcase1C :isa(    Testcase1A    ) { }

    my $objc = Testcase1C->new;
    ok($objc isa Testcase1A, ':isa attribute trims whitespace');
}

{
    class Testcase2A 1.23 { }

    class Testcase2B :isa(Testcase2A 1.0) { } # OK

    ok(!defined eval "class Testcase2C :isa(Testcase2A 2.0) {}; 1",
        ':isa() version test can throw');
    like($@, qr/^Testcase2A version 2\.0 required--this is only version 1\.23 at /,
        'Exception thrown from :isa version test');
}

{
    class Testcase3A {
        field $x :param;
        method x { return $x; }
    }

    class Testcase3B :isa(Testcase3A) {}

    my $obj = Testcase3B->new(x => "X");
    is($obj->x, "X", 'Constructor params passed through to superclass');
}

{
    class Testcase4A { }

    class Testcase4B :isa(Testcase4A);

    package main;
    my $obj = Testcase4B->new;
    ok($obj isa Testcase4A, 'Unit class syntax allows :isa');
}

{
    class Testcase5A {
        field $classname = __CLASS__;
        method classname { return $classname }
    }

    class Testcase5B :isa(Testcase5A) { }

    is(Testcase5B->new->classname, "Testcase5B", '__CLASS__ yields correct class name for subclass');
}

{
    # https://github.com/Perl/perl5/issues/21332
    use lib 'lib/class';
    ok(eval <<'EOS', "hierarchical base class loaded");
use A::B;
1;
EOS
}

{
    # https://github.com/Perl/perl5/issues/20891
    class Testcase6A 1.23 {}
    class Testcase6B 1.23 :isa(Testcase6A) {}

    ok(Testcase6B->new isa Testcase6A, 'Testcase6B inherits Testcase6B');
    is(Testcase6B->VERSION, 1.23, 'Testcase6B sets VERSION');
}

done_testing;
