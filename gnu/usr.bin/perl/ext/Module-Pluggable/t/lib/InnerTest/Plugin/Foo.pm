package InnerTest::Plugin::Foo;
use strict;

our $FOO = 1;

package InnerTest::Plugin::Bar;
use strict;

sub bar {}

package InnerTest::Plugin::Quux;
use strict;
use base qw(InnerTest::Plugin::Bar);



1;
