#!./perl

package Foo;

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan 7;

use constant MyClass => 'Foo::Bar::Biz::Baz';

{
    package Foo::Bar::Biz::Baz;
    1;
}

for (qw(Foo Foo:: MyClass __PACKAGE__)) {
    eval "sub { my $_ \$obj = shift; }";
    ok ! $@;
#    print $@ if $@;
}

use constant NoClass => 'Nope::Foo::Bar::Biz::Baz';

for (qw(Nope Nope:: NoClass)) {
    eval "sub { my $_ \$obj = shift; }";
    ok $@;
#    print $@ if $@;
}
