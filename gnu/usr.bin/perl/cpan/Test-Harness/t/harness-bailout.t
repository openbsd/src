#!perl

use strict;
use File::Spec;

BEGIN {
    *CORE::GLOBAL::exit = sub { die '!exit called!' };
}

use TAP::Harness;
use Test::More;

my @jobs = (
    {   name => 'sequential',
        args => { verbosity => -9 },
    },
    {   name => 'parallel',
        args => { verbosity => -9, jobs => 2 },
    },
);

plan tests => @jobs * 2;

for my $test (@jobs) {
    my $name    = $test->{name};
    my $args    = $test->{args};
    my $harness = TAP::Harness->new($args);
    eval {
        local ( *OLDERR, *OLDOUT );
        open OLDERR, '>&STDERR' or die $!;
        open OLDOUT, '>&STDOUT' or die $!;
        my $devnull = File::Spec->devnull;
        open STDERR, ">$devnull" or die $!;
        open STDOUT, ">$devnull" or die $!;

        $harness->runtests(
            File::Spec->catfile(
                't',
                'sample-tests',
                'bailout'
            )
        );

        open STDERR, '>&OLDERR' or die $!;
        open STDOUT, '>&OLDOUT' or die $!;
    };
    my $err = $@;
    unlike $err, qr{!exit called!}, "$name: didn't exit";
    like $err, qr{FAILED--Further testing stopped: GERONIMMMOOOOOO!!!},
      "$name: bailout message";
}

# vim:ts=2:sw=2:et:ft=perl

