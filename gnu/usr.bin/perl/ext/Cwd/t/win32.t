#!./perl

BEGIN {
    chdir 't' if -d 't';
    if ($ENV{PERL_CORE}) {
        @INC = '../lib';
    }
}

use Test::More;
if( $^O eq 'MSWin32' ) {
  plan tests => 3;
} else {
  plan skip_all => 'this is not win32';
}

use Cwd;
ok 1;

my $cdir = getdcwd('C:');
like $cdir, qr{^C:};

my $ddir = getdcwd('D:');
if (defined $ddir) {
  like $ddir, qr{^D:};
} else {
  # May not have a D: drive mounted
  ok 1;
}
