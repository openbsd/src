package MyTest::Plugin::Foo;


use strict;

sub new { return bless {}, $_[0]; }
sub frobnitz {}
1;


