# -*- mode: perl; -*-

use strict;
use warnings;

use Test::More tests => 126;

# Test Math::BigInt->config(), Math::BigFloat->config(), and
# Math::BigRat->config().

use Math::BigInt lib => 'Calc';
use Math::BigFloat;
use Math::BigRat;

my $mbi = 'Math::BigInt';
my $mbf = 'Math::BigFloat';
my $mbr = 'Math::BigRat';

my @classes = ($mbi, $mbf, $mbr);

# Default configuration.

my %defaults = (
  'accuracy'    => undef,
  'precision'   => undef,
  'round_mode'  => 'even',
  'div_scale'   => 40,
  'trap_inf'    => 0,
  'trap_nan'    => 0,
  'upgrade'     => undef,
  'downgrade'   => undef,
  'lib'         => 'Math::BigInt::Calc',
  'lib_version' => $Math::BigInt::Calc::VERSION,
);

##############################################################################
# Test config() as a getter.

for my $class (@classes) {

    note <<"EOF";

Verify that $class->config("key") works.

EOF

    can_ok($class, 'config');

    my %table = (%defaults, 'class' => $class);

    # Test getting via the new style $class->config($key).

    subtest qq|New-style getter $class->config("\$key")| => sub {
        plan tests => scalar keys %table;

        for my $key (sort keys %table) {
            my $val = $table{$key};
            is($class->config($key), $val, qq|$class->config("$key")|);
        }
    };

    # Test getting via the old style $class->config()->{$key}, which is still
    # supported:

    my $cfg = $class->config();
    is(ref($cfg), 'HASH', "ref() of output from $class->config()");

    subtest qq|Old-style getter $class->config()->{"\$key"}| => sub {
        plan tests => scalar keys %table;

       for my $key (sort keys %table) {
            my $val = $table{$key};
            is($cfg->{$key}, $val, qq|$class->config()->{$key}|);
        }
    };

    # Can set via hash ref?

    $cfg = $class->config({ trap_nan => 1 });
    is($cfg->{trap_nan}, 1, 'can set "trap_nan" to 1 via hash ref');

    # Restore for later.

    $cfg = $class->config({ trap_nan => 0 });
    is($cfg->{trap_nan}, 0, 'can set "trap_nan" to 0 via hash ref');
}

##############################################################################
# Test config() as a setter.

# Alternative configuration. All values should be different from the default
# configuration.

my $test = {
    accuracy   => 2,
    precision  => 3,
    round_mode => 'zero',
    div_scale  => '100',
    trap_inf   => 1,
    trap_nan   => 1,
    upgrade    => 'Math::BigInt::SomeClass',
    downgrade  => 'Math::BigInt::SomeClass',
};

for my $class (@classes) {

    note <<"EOF";

Verify that $class->config("key" => value) works and that
it doesn't affect the configuration of other classes.

EOF

    for my $key (sort keys %$test) {

        # Get the original value for restoring it later.

        my $orig = $class->config($key);

        # Try setting the new value.

        eval { $class->config($key => $test->{$key}); };
        die $@ if $@;

        # Verify that the value was set correctly.

        is($class->config($key), $test->{$key},
           qq|$class->config("$key") set to $test->{$key}|);

        # Verify that setting it in class $class didn't affect other classes.

        for my $other (@classes) {
            next if $other eq $class;

            isnt($other->config($key), $class->config($key),
                 qq|$other->config("$key") isn't affected by setting | .
                 qq|$class->config("$key")|);
        }

        # Restore the value.

        $class->config($key => $orig);

        # Verify that the value was restored.

        is($class->config($key), $orig,
           qq|$class->config("$key") reset to | .
           (defined($orig) ? qq|"$orig"| : "undef"));
    }
}

# Verify that setting via a hash doesn't modify the hash.

# In the $test configuration, both accuracy and precision are defined, which
# won't work, so set one of them to undef.

$test->{accuracy} = undef;

for my $class (@classes) {

    note <<"EOF";

Verify that $class->config({key1 => val1, key2 => val2, ...})
doesn't modify the hash ref argument.

EOF

    subtest "Verify that $class->config(\$cfg) doesn't modify \$cfg" => sub {
        plan tests => 2 * keys %$test;

        # Make copy of the configuration hash and use it as input to config().

        my $cfg = { %{ $test } };
        eval { $class -> config($cfg); };
        die $@ if $@;

        # Verify that the configuration hash hasn't been modified.

        for my $key (sort keys %$test) {
            ok(exists $cfg->{$key}, qq|existens of \$cfg->{"$key"}|);
            is($cfg->{$key}, $test->{$key}, qq|value of \$cfg->{"$key"}|);
        }
    };
}

# Special testing of setting both accuracy and precision simultaneouly with
# config(). This didn't work correctly before.

for my $class (@classes) {

    note <<"EOF";

Verify that $class->config({accuracy => \$a, precision => \$p})
works as intended.

EOF

    $class -> config({"accuracy" => 4, "precision" => undef});

    subtest qq|$class -> config({"accuracy" => 4, "precision" => undef})|
      => sub {
          plan tests => 2;

          is($class -> config("accuracy"), 4,
             qq|$class -> config("accuracy")|);
          is($class -> config("precision"), undef,
             qq|$class -> config("precision")|);
      };

    $class -> config({"accuracy" => undef, "precision" => 5});

    subtest qq|$class -> config({"accuracy" => undef, "precision" => 5})|
      => sub {
          plan tests => 2;

          is($class -> config("accuracy"), undef,
             qq|$class -> config("accuracy")|);
          is($class -> config("precision"), 5,
             qq|$class -> config("precision")|);
      };
}

# Test getting an invalid key (should croak)

note <<"EOF";

Verify behaviour when getting an invalid key.

EOF

for my $class (@classes) {
    eval { $class->config('some_garbage' => 1); };
    like($@,
         qr/ ^ Illegal \s+ key\(s\) \s+ 'some_garbage' \s+ passed \s+ to \s+
             $class->config\(\) \s+ at
           /x,
         "Passing invalid key to $class->config() causes an error.");
}
