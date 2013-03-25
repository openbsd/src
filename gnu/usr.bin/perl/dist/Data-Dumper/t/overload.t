#!./perl -w

BEGIN {
    if ($ENV{PERL_CORE}){
        require Config; import Config;
        no warnings 'once';
        if ($Config{'extensions'} !~ /\bData\/Dumper\b/) {
            print "1..0 # Skip: Data::Dumper was not built\n";
            exit 0;
        }
    }
}

use strict;
use Data::Dumper;

use Test::More tests => 4;

package Foo;
use overload '""' => 'as_string';

sub new { bless { foo => "bar" }, shift }
sub as_string { "%%%%" }

package main;

my $f = Foo->new;

isa_ok($f, 'Foo');
is("$f", '%%%%', 'String overloading works');

my $d = Dumper($f);

like($d, qr/bar/);
like($d, qr/Foo/);

