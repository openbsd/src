# -*- mode: perl; -*-

use strict;
use warnings;

# Test 2 levels of upgrade classes. This used to cause a segv.

use Test::More tests => 9;

use Math::BigInt upgrade => 'Math::BigFloat';
use Math::BigFloat upgrade => 'Math::BigMouse';

no warnings 'once';
@Math::BigMouse::ISA = 'Math::BigFloat';
sub Math::BigMouse::bsqrt {};

() = sqrt Math::BigInt->new(2);
pass('sqrt on a big int does not segv if there are 2 upgrade levels');

# Math::BigRat inherits from Math::BigFloat, which inherits from Math::BigInt.
# Typically, methods call the upgrade version if upgrading is defined and the
# argument is an unknown type. This will call infinite recursion for methods
# that are not implemented in the upgrade class.

use Math::BigRat;

Math::BigFloat -> upgrade("Math::BigRat");
Math::BigFloat -> downgrade(undef);

Math::BigRat   -> upgrade(undef);
Math::BigRat   -> downgrade(undef);

# Input is a scalar.

note 'Math::BigRat -> babs("2");';
()  = Math::BigRat -> babs("2");
pass(qq|no 'Deep recursion on subroutine ...'|);

note 'Math::BigRat -> bsgn("2");';
()  = Math::BigRat -> bsgn("2");
pass(qq|no 'Deep recursion on subroutine ...'|);

# Input is a Math::BigInt.

note 'Math::BigRat -> babs(Math::BigInt -> new("2"));';
()  = Math::BigRat -> babs(Math::BigInt -> new("2"));
pass(qq|no 'Deep recursion on subroutine ...'|);

note 'Math::BigRat -> bsgn(Math::BigInt -> new("2"));';
()  = Math::BigRat -> bsgn(Math::BigInt -> new("2"));
pass(qq|no 'Deep recursion on subroutine ...'|);

# Input is a Math::BigFloat.

note 'Math::BigRat -> babs(Math::BigFloat -> new("2"));';
()  = Math::BigRat -> babs(Math::BigFloat -> new("2"));
pass(qq|no 'Deep recursion on subroutine ...'|);

note 'Math::BigRat -> bsgn(Math::BigFloat -> new("2"));';
()  = Math::BigRat -> bsgn(Math::BigFloat -> new("2"));
pass(qq|no 'Deep recursion on subroutine ...'|);

# Input is a Math::BigRat.

note 'Math::BigRat -> babs(Math::BigRat -> new("2"));';
()  = Math::BigRat -> babs(Math::BigRat -> new("2"));
pass(qq|no 'Deep recursion on subroutine ...'|);

note 'Math::BigRat -> bsgn(Math::BigRat -> new("2"));';
()  = Math::BigRat -> bsgn(Math::BigRat -> new("2"));
pass(qq|no 'Deep recursion on subroutine ...'|);
