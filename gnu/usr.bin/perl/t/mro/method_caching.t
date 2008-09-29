#!./perl

use strict;
use warnings;
no warnings 'redefine'; # we do a lot of this
no warnings 'prototype'; # we do a lot of this

BEGIN {
    unless (-d 'blib') {
        chdir 't' if -d 't';
        @INC = '../lib';
    }
}

require './test.pl';

{
    package MCTest::Base;
    sub foo { return $_[1]+1 };

    package MCTest::Derived;
    our @ISA = qw/MCTest::Base/;

    package Foo; our @FOO = qw//;
}

# These are various ways of re-defining MCTest::Base::foo and checking whether the method is cached when it shouldn't be
my @testsubs = (
    sub { is(MCTest::Derived->foo(0), 1); },
    sub { eval 'sub MCTest::Base::foo { return $_[1]+2 }'; is(MCTest::Derived->foo(0), 2); },
    sub { eval 'sub MCTest::Base::foo($) { return $_[1]+3 }'; is(MCTest::Derived->foo(0), 3); },
    sub { eval 'sub MCTest::Base::foo($) { 4 }'; is(MCTest::Derived->foo(0), 4); },
    sub { *MCTest::Base::foo = sub { $_[1]+5 }; is(MCTest::Derived->foo(0), 5); },
    sub { local *MCTest::Base::foo = sub { $_[1]+6 }; is(MCTest::Derived->foo(0), 6); },
    sub { is(MCTest::Derived->foo(0), 5); },
    sub { sub FFF { $_[1]+7 }; local *MCTest::Base::foo = *FFF; is(MCTest::Derived->foo(0), 7); },
    sub { is(MCTest::Derived->foo(0), 5); },
    sub { sub DDD { $_[1]+8 }; *MCTest::Base::foo = *DDD; is(MCTest::Derived->foo(0), 8); },
    sub { *ASDF::asdf = sub { $_[1]+9 }; *MCTest::Base::foo = \&ASDF::asdf; is(MCTest::Derived->foo(0), 9); },
    sub { undef *MCTest::Base::foo; eval { MCTest::Derived->foo(0) }; like($@, qr/locate object method/); },
    sub { eval "sub MCTest::Base::foo($);"; *MCTest::Base::foo = \&ASDF::asdf; is(MCTest::Derived->foo(0), 9); },
    sub { *XYZ = sub { $_[1]+10 }; ${MCTest::Base::}{foo} = \&XYZ; is(MCTest::Derived->foo(0), 10); },
    sub { ${MCTest::Base::}{foo} = sub { $_[1]+11 }; is(MCTest::Derived->foo(0), 11); },

    sub { undef *MCTest::Base::foo; eval { MCTest::Derived->foo(0) }; like($@, qr/locate object method/); },
    sub { eval 'package MCTest::Base; sub foo { $_[1]+12 }'; is(MCTest::Derived->foo(0), 12); },
    sub { eval 'package ZZZ; sub foo { $_[1]+13 }'; *MCTest::Base::foo = \&ZZZ::foo; is(MCTest::Derived->foo(0), 13); },
    sub { ${MCTest::Base::}{foo} = sub { $_[1]+14 }; is(MCTest::Derived->foo(0), 14); },
    # 5.8.8 fails this one
    sub { undef *{MCTest::Base::}; eval { MCTest::Derived->foo(0) }; like($@, qr/locate object method/); },
    sub { eval 'package MCTest::Base; sub foo { $_[1]+15 }'; is(MCTest::Derived->foo(0), 15); },
    sub { undef %{MCTest::Base::}; eval { MCTest::Derived->foo(0) }; like($@, qr/locate object method/); },
    sub { eval 'package MCTest::Base; sub foo { $_[1]+16 }'; is(MCTest::Derived->foo(0), 16); },
    sub { %{MCTest::Base::} = (); eval { MCTest::Derived->foo(0) }; like($@, qr/locate object method/); },
    sub { eval 'package MCTest::Base; sub foo { $_[1]+17 }'; is(MCTest::Derived->foo(0), 17); },
    # 5.8.8 fails this one too
    sub { *{MCTest::Base::} = *{Foo::}; eval { MCTest::Derived->foo(0) }; like($@, qr/locate object method/); },
    sub { *MCTest::Derived::foo = \&MCTest::Base::foo; eval { MCTest::Derived::foo(0,0) }; ok(!$@); undef *MCTest::Derived::foo },
    sub { eval 'package MCTest::Base; sub foo { $_[1]+18 }'; is(MCTest::Derived->foo(0), 18); },
);

plan(tests => scalar(@testsubs));

$_->() for (@testsubs);
