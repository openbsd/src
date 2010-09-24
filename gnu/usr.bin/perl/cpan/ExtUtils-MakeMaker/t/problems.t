# Test problems in Makefile.PL's and hint files.

BEGIN {
    unshift @INC, 't/lib';
}
chdir 't';

use strict;
use Test::More tests => 6;
use ExtUtils::MM;
use MakeMaker::Test::Setup::Problem;
use TieOut;

my $MM = bless { DIR => ['subdir'] }, 'MM';

ok( setup_recurs(), 'setup' );
END {
    ok( chdir File::Spec->updir );
    ok( teardown_recurs(), 'teardown' );
}

ok( chdir 'Problem-Module', "chdir'd to Problem-Module" ) ||
  diag("chdir failed: $!");


# Make sure when Makefile.PL's break, they issue a warning.
# Also make sure Makefile.PL's in subdirs still have '.' in @INC.
{
    my $stdout = tie *STDOUT, 'TieOut' or die;

    my $warning = '';
    local $SIG{__WARN__} = sub { $warning = join '', @_ };
    eval { $MM->eval_in_subdirs; };

    is( $stdout->read, qq{\@INC has .\n}, 'cwd in @INC' );
    like( $@, 
          qr{^ERROR from evaluation of .*subdir.*Makefile.PL: YYYAaaaakkk},
          'Makefile.PL death in subdir warns' );

    untie *STDOUT;
}
