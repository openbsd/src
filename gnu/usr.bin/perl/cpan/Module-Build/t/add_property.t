#!/usr/bin/perl -w

use strict;
use lib 't/lib';
use MBTest tests => 27;
#use MBTest 'no_plan';
use DistGen;

blib_load 'Module::Build';

my $tmp = MBTest->tmpdir;
my $dist = DistGen->new( dir => $tmp );
$dist->regen;
$dist->chdir_in;

ADDPROP: {
  package My::Build::Prop;
  use base 'Module::Build';
  __PACKAGE__->add_property( 'foo' );
  __PACKAGE__->add_property( 'bar', 'howdy' );
  __PACKAGE__->add_property( 'baz', default => 'howdy' );
  __PACKAGE__->add_property( 'code', default => sub { 'yay' } );
  __PACKAGE__->add_property(
    'check',
    default => sub { 'howdy' },
    check   => sub {
      return 1 if $_ eq 'howdy';
      shift->property_error(qq{"$_" is invalid});
      return 0;
    },
  );
  __PACKAGE__->add_property(
    'hash',
    default => { foo => 1 },
    check   => sub {
      return 1 if !defined $_ or exists $_->{foo};
      shift->property_error(qq{hash is invalid});
      return 0;
    },
  );
}

ok my $build = My::Build::Prop->new(
  'module_name' => 'Simple',
  quiet => 1,
), 'Create new build object';

is $build->foo, undef, 'Property "foo" should be undef';
ok $build->foo(42), 'Set "foo"';
is $build->foo, 42, 'Now "foo" should have new value';

is $build->bar, 'howdy', 'Property "bar" should be its default';
ok $build->bar('yo'), 'Set "bar"';
is $build->bar, 'yo', 'Now "bar" should have new value';

is $build->check, 'howdy', 'Property "check" should be its default';

eval { $build->check('yo') };
ok my $err = $@, 'Should get an error for an invalid value';
like $err, qr/^ERROR: "yo" is invalid/, 'It should be the correct error';

is $build->code, 'yay', 'Property "code" should have its code value';

is_deeply $build->hash, { foo => 1 }, 'Property "hash" should be default';
is $build->hash('foo'), 1, 'Should be able to get key in hash';
ok $build->hash( bar => 3 ), 'Add a key to the hash prop';
is_deeply $build->hash, { foo => 1, bar => 3 }, 'New key should be in hash';

eval { $build->hash({ bar => 3 }) };
ok $err = $@, 'Should get exception for assigning invalid hash';
like $err, qr/^ERROR: hash is invalid/, 'It should be the correct error';

eval { $build->hash( []) };
ok $err = $@, 'Should get exception for assigning an array for a hash';
like $err, qr/^Unexpected arguments for property 'hash'/,
  'It should be the proper error';
is $build->hash(undef), undef, 'Should be able to set hash to undef';

# Check core properties.
is $build->installdirs, 'site', 'Property "installdirs" should be default';
ok $build->installdirs('core'), 'Set "installdirst" to "core"';
is $build->installdirs, 'core', 'Now "installdirs" should be "core"';

eval { $build->installdirs('perl') };
ok $err = $@, 'Should have caught exception setting "installdirs" to "perl"';
like $err, qr/^ERROR: Perhaps you meant installdirs to be "core" rather than "perl"\?/,
  'And it should suggest "core" in the error message';

eval { $build->installdirs('foo') };
ok $err = $@, 'Should catch exception for invalid "installdirs" value';
like $err, qr/ERROR: installdirs must be one of "core", "site", or "vendor"/,
  'And it should suggest the proper values in the error message';

$dist->chdir_original if $dist->did_chdir;
