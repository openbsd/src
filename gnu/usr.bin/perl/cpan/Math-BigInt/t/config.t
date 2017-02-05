#!perl

use strict;
use warnings;

use Test::More tests => 55;

# test whether Math::BigInt->config() and Math::BigFloat->config() works

use Math::BigInt lib => 'Calc';
use Math::BigFloat;

my $mbi = 'Math::BigInt';
my $mbf = 'Math::BigFloat';

##############################################################################
# Math::BigInt

{
    can_ok($mbi, 'config');

    my $cfg = $mbi->config();

    is(ref($cfg), 'HASH', 'ref() of output from $mbi->config()');

    is($cfg->{lib}, 'Math::BigInt::Calc', 'lib');
    is($cfg->{lib_version}, $Math::BigInt::Calc::VERSION, 'lib_version');
    is($cfg->{class}, $mbi, 'class');
    is($cfg->{upgrade} || '', '', 'upgrade');
    is($cfg->{div_scale}, 40, 'div_Scale');

    is($cfg->{precision} || 0, 0, 'precision'); # should test for undef
    is($cfg->{accuracy} || 0, 0, 'accuracy');
    is($cfg->{round_mode}, 'even', 'round_mode');

    is($cfg->{trap_nan}, 0, 'trap_nan');
    is($cfg->{trap_inf}, 0, 'trap_inf');

    is($mbi->config('lib'), 'Math::BigInt::Calc', 'config("lib")');

    # can set via hash ref?
    $cfg = $mbi->config({ trap_nan => 1 });
    is($cfg->{trap_nan}, 1, 'can set "trap_nan" via hash ref');

    # reset for later
    $mbi->config(trap_nan => 0);
}

##############################################################################
# Math::BigFloat

{
    can_ok($mbf, 'config');

    my $cfg = $mbf->config();

    is(ref($cfg), 'HASH', 'ref() of output from $mbf->config()');

    is($cfg->{lib}, 'Math::BigInt::Calc', 'lib');
    is($cfg->{with}, 'Math::BigInt::Calc', 'with');
    is($cfg->{lib_version}, $Math::BigInt::Calc::VERSION, 'lib_version');
    is($cfg->{class}, $mbf, 'class');
    is($cfg->{upgrade} || '', '', 'upgrade');
    is($cfg->{div_scale}, 40, 'div_Scale');

    is($cfg->{precision} || 0, 0, 'precision'); # should test for undef
    is($cfg->{accuracy} || 0, 0, 'accuracy');
    is($cfg->{round_mode}, 'even', 'round_mode');

    is($cfg->{trap_nan}, 0, 'trap_nan');
    is($cfg->{trap_inf}, 0, 'trap_inf');

    is($mbf->config('lib'), 'Math::BigInt::Calc', 'config("lib")');

    # can set via hash ref?
    $cfg = $mbf->config({ trap_nan => 1 });
    is($cfg->{trap_nan}, 1, 'can set "trap_nan" via hash ref');

    # reset for later
    $mbf->config(trap_nan => 0);
}

##############################################################################
# test setting values

my $test = {
    trap_nan   => 1,
    trap_inf   => 1,
    accuracy   => 2,
    precision  => 3,
    round_mode => 'zero',
    div_scale  => '100',
    upgrade    => 'Math::BigInt::SomeClass',
    downgrade  => 'Math::BigInt::SomeClass',
};

my $c;

foreach my $key (keys %$test) {

    # see if setting in MBI works
    eval { $mbi->config($key => $test->{$key}); };
    $c = $mbi->config();
    is("$key = $c->{$key}", "$key = $test->{$key}", "$key = $test->{$key}");
    $c = $mbf->config();

    # see if setting it in MBI leaves MBF alone
    ok(($c->{$key} || 0) ne $test->{$key},
       "$key ne \$c->{$key}");

    # see if setting in MBF works
    eval { $mbf->config($key => $test->{$key}); };
    $c = $mbf->config();
    is("$key = $c->{$key}", "$key = $test->{$key}", "$key = $test->{$key}");
}

##############################################################################
# test setting illegal keys (should croak)

eval { $mbi->config('some_garbage' => 1); };
like($@,
     qr/ ^ Illegal \s+ key\(s\) \s+ 'some_garbage' \s+ passed \s+ to \s+
         Math::BigInt->config\(\) \s+ at
       /x,
     'Passing invalid key to Math::BigInt->config() causes an error.');

eval { $mbf->config('some_garbage' => 1); };
like($@,
     qr/ ^ Illegal \s+ key\(s\) \s+ 'some_garbage' \s+ passed \s+ to \s+
         Math::BigFloat->config\(\) \s+ at
       /x,
     'Passing invalid key to Math::BigFloat->config() causes an error.');
