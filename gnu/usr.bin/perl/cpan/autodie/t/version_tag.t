#!/usr/bin/perl -w
use strict;
use warnings;
use Test::More tests => 5;
use constant NO_SUCH_FILE => 'THIS_FILE_HAD_BETTER_NOT_EXIST';

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

eval {
    use autodie qw(:2.07);

    # 2.07 didn't support chmod.  This shouldn't throw an
    # exception.

    chmod(0644,NO_SUCH_FILE);
};

is($@,"","chmod wasn't supported in 2.07");

eval {
    use autodie;

    chmod(0644,NO_SUCH_FILE);
};

isa_ok($@, 'autodie::exception', 'Our current version supports chmod');
