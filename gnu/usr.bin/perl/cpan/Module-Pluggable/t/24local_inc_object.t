#!perl -w

use strict;
use FindBin;
use Test::More tests => 2;

my $inc  = IncTest->new();
my ($ta) = grep { ref($_) eq 'Text::Abbrev'} eval { local ($^W) = 0; $inc->plugins };
ok($ta);
is($ta->MPCHECK, "HELLO");

package IncTest;
use Module::Pluggable search_path => "Text", 
                      search_dirs => "t/lib", 
                      instantiate => 'module_pluggable', 
                      on_require_error     => sub { },
                      on_instantiate_error => sub { };

sub new {
    my $class = shift;
    return bless {}, $class;
}
1;
