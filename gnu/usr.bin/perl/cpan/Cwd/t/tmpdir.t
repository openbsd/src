use strict;
use Test;

# Grab all of the plain routines from File::Spec
use File::Spec;
use File::Spec::Win32;

plan tests => 4;

ok 1, 1, "Loaded";

if ($^O eq 'VMS') {
    # hack:
    # Need to cause the %ENV to get populated or you only get the builtins at
    # first, and then something else can cause the hash to get populated.
    my %look_env = %ENV;
}
my $num_keys = keys %ENV;
File::Spec->tmpdir;
ok scalar keys %ENV, $num_keys, "tmpdir() shouldn't change the contents of %ENV";

if ($^O eq 'VMS') {
  skip("Can't make list assignment to %ENV on this system", 1);
} else {
  local %ENV;
  File::Spec::Win32->tmpdir;
  ok scalar keys %ENV, 0, "Win32->tmpdir() shouldn't change the contents of %ENV";
}

File::Spec::Win32->tmpdir;
ok scalar keys %ENV, $num_keys, "Win32->tmpdir() shouldn't change the contents of %ENV";
