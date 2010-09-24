#!/usr/bin/perl -w

BEGIN {
    if ($^O eq 'VMS') {
        print '1..0 # Child test output confuses harness';
        exit;
    }
}

use strict;
use lib 't/lib';
use MBTest tests => 7;

blib_load('Module::Build');

my $tmp = MBTest->tmpdir;

use DistGen;

my $dist = DistGen->new( dir => $tmp );


$dist->add_file('t/special_ext.st', <<'---' );
#!perl
use Test::More tests => 2;
ok(1, 'first test in special_ext');
ok(1, 'second test in special_ext');
---

$dist->regen;

$dist->chdir_in;

#########################

# Here we make sure we can define an action that will test a particular type
$::x = 0;
my $mb = Module::Build->subclass(
    code => q#
        sub ACTION_testspecial {
            $::x++;
            shift->generic_test(type => 'special');
        }
    #
)->new(
    module_name => $dist->name,
    test_types  => { special => '.st' }
);

ok $mb;

$mb->dispatch('testspecial');
is($::x, 1, "called once");


$mb->add_to_cleanup('save_out');
# Use uc() so we don't confuse the current test output
my $verbose_output = uc(stdout_of(
    sub {$mb->dispatch('testspecial', verbose => 1)}
));

like($verbose_output, qr/^OK 1 - FIRST TEST IN SPECIAL_EXT/m);
like($verbose_output, qr/^OK 2 - SECOND TEST IN SPECIAL_EXT/m);

is( $::x, 2, "called again");

my $output = uc(stdout_of(
    sub {$mb->dispatch('testspecial', verbose => 0)}
));
like($output, qr/\.\. ?OK/);

is($::x, 3, "called a third time");

# vim:ts=4:sw=4:et:sta
