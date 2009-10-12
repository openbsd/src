#!/usr/bin/perl -w
use strict;
use warnings;
use Test::More tests => 3;

eval {
    use autodie qw(:1.994);

    open(my $fh, '<', 'this_file_had_better_not_exist.txt');
};

isa_ok($@, 'autodie::exception', "Basic version tags work");


# Expanding :1.00 should fail, there was no autodie :1.00
eval { my $foo = autodie->_expand_tag(":1.00"); };

isnt($@,"","Expanding :1.00 should fail");

my $version = $autodie::VERSION;

# Expanding our current version should work!
eval { my $foo = autodie->_expand_tag(":$version"); };

is($@,"","Expanding :$version should succeed");

