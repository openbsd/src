#!/usr/bin/perl

use strict;
use warnings;

use Test::Builder::Tester tests => 3;
use Test::More;
use File::Basename qw(dirname);
use File::Spec qw();

my $file = File::Spec->join(dirname(__FILE__), 'tbt_09do_script.pl');
my $done = do $file;
ok(defined($done), 'do succeeded') or do {
    if ($@) {
        diag qq(  \$@ is '$@'\n);
    } elsif ($!) {
        diag qq(  \$! is '$!'\n);
    } else {
        diag qq(  file's last statement returned undef: $file)
    }
};
