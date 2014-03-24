#!perl -w

use strict;
use FindBin;
use Test::More tests => 1;

IncTest->new()->plugins;
is(Text::Abbrev->MPCHECK, "HELLO");

package IncTest;
use Module::Pluggable search_path => "Text", search_dirs => "t/lib", require => 1;

sub new {
    my $class = shift;
    return bless {}, $class;
}
1;