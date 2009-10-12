#!/usr/bin/perl -w

use strict;
use Test::More;
use lib $ENV{PERL_CORE} ? '../lib/Module/Build/t/lib' : 't/lib';
if (eval { require TAP::Harness } && TAP::Harness->VERSION >= 3) {
    plan tests => 8;
} else {
    plan skip_all => 'TAP::Harness 3+ not installed'
}

use MBTest;
use DistGen;

use_ok 'Module::Build';
ensure_blib('Module::Build');
my $tmp = MBTest->tmpdir;
my $dist = DistGen->new( dir => $tmp );
$dist->regen;

$dist->chdir_in;
#########################

# Make sure that TAP::Harness properly does its thing.
ok my $mb = Module::Build->new(
    module_name     => $dist->name,
    use_tap_harness => 1,
    quiet           => 1,
), 'Construct build object with test_file_exts parameter';

$mb->add_to_cleanup('save_out');
# Use uc() so we don't confuse the current test output
my $out = uc(stdout_of(
    sub {$mb->dispatch('test', verbose => 1)}
));

like $out, qr/^OK 1/m, 'Should see first test output';
like $out, qr/^ALL TESTS SUCCESSFUL/m, 'Should see test success message';

#########################

# Make sure that arguments are passed through to TAP::Harness.
ok $mb = Module::Build->new(
    module_name     => $dist->name,
    use_tap_harness => 1,
    tap_harness_args => { verbosity => 0 },
    quiet           => 1,
), 'Construct build object with test_file_exts parameter';

$mb->add_to_cleanup('save_out');
# Use uc() so we don't confuse the current test output
$out = uc(stdout_of(
    sub {$mb->dispatch('test', verbose => 1)}
));

unlike $out, qr/^OK 1/m, 'Should not see first test output';
like $out, qr/^ALL TESTS SUCCESSFUL/m, 'Should see test success message';

$dist->remove;

# vim:ts=4:sw=4:et:sta
