#!/usr/bin/perl -w

use strict;
use lib $ENV{PERL_CORE} ? '../lib/Module/Build/t/lib' : 't/lib';
use MBTest tests => 5;
use DistGen;

use_ok 'Module::Build';
ensure_blib('Module::Build');

my $tmp = MBTest->tmpdir;
my $dist = DistGen->new( dir => $tmp );

$dist->add_file('t/mytest.s', <<'---' );
#!perl
use Test::More tests => 2;
ok(1, 'first mytest.s');
ok(1, 'second mytest.s');
---

$dist->regen;
$dist->chdir_in;

#########################

# So make sure that the test gets run with the alternate extension.
ok my $mb = Module::Build->new(
    module_name    => $dist->name,
    test_file_exts => ['.s'],
    quiet          => 1,
), 'Construct build object with test_file_exts parameter';

$mb->add_to_cleanup('save_out');
# Use uc() so we don't confuse the current test output
my $out = uc(stdout_of(
    sub {$mb->dispatch('test', verbose => 1)}
));

like $out, qr/^OK 1 - FIRST MYTEST[.]S/m, 'Should see first test output';
like $out, qr/^OK 2 - SECOND MYTEST[.]S/m, 'Should see second test output';

# Cleanup.
$dist->remove;

# vim:ts=4:sw=4:et:sta
