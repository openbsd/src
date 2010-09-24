#!/usr/bin/perl -w

use strict;
use Test::More;
use lib 't/lib';
if (eval { require TAP::Harness } && TAP::Harness->VERSION >= 3) {
    plan tests => 9;
} else {
    plan skip_all => 'TAP::Harness 3+ not installed'
}

use MBTest;
use DistGen;

blib_load('Module::Build');
my $tmp = MBTest->tmpdir;
my $dist = DistGen->new( dir => $tmp );
$dist->regen;
$dist->chdir_in;

#########################

# Make sure that TAP::Harness properly does its thing.
$dist->change_build_pl(
    module_name     => $dist->name,
    use_tap_harness => 1,
    quiet           => 1,
);
$dist->regen;

ok my $mb = $dist->new_from_context,
    'Construct build object with test_file_exts parameter';

$mb->add_to_cleanup('save_out');
# Use uc() so we don't confuse the current test output
my $out = uc(stdout_of(
    sub {$mb->dispatch('test', verbose => 1)}
));

like $out, qr/^OK 1/m, 'Should see first test output';
like $out, qr/^ALL TESTS SUCCESSFUL/m, 'Should see test success message';

#########################

# Make sure that arguments are passed through to TAP::Harness.
$dist->change_build_pl(
    module_name     => $dist->name,
    use_tap_harness => 1,
    tap_harness_args => { verbosity => 0 },
    quiet           => 1,
);
$dist->regen;

ok $mb = $dist->new_from_context,
    'Construct build object with test_file_exts parameter';

$mb->add_to_cleanup('save_out');
# Use uc() so we don't confuse the current test output
$out = uc(stdout_of(
    sub {$mb->dispatch('test', verbose => 1)}
));

unlike $out, qr/^OK 1/m, 'Should not see first test output';
like $out, qr/^ALL TESTS SUCCESSFUL/m, 'Should see test success message';

#--------------------------------------------------------------------------#
# test that a failing test dies
#--------------------------------------------------------------------------#

$dist->change_build_pl(
    module_name     => $dist->name,
    use_tap_harness => 1,
    tap_harness_args => { verbosity => 1 },
    quiet           => 1,
);
$dist->change_file('t/basic.t',<<"---");
use Test::More tests => 1;
use strict;

use $dist->{name};
ok 0;
---
$dist->regen;

ok $mb = $dist->new_from_context,
    'Construct build object after setting tests to fail';
# Use uc() so we don't confuse the current test output
$out = stdout_stderr_of( sub { $dist->run_build('test')} );
ok( $?, "'Build test' had non-zero exit code" );
like( $out, qr{Errors in testing\.  Cannot continue\.},
    "Saw emulated Test::Harness die() message"
);

# vim:ts=4:sw=4:et:sta
