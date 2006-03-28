#!./perl -Tw
# Testing Cwd under taint mode.

use strict;

BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir 't';
        @INC = '../lib';
    }
}
use Cwd;
chdir 't';

use File::Spec;
use lib File::Spec->catdir('t', 'lib');
use Test::More tests => 17;

use Scalar::Util qw/tainted/;

my @Functions = qw(getcwd cwd fastcwd fastgetcwd
                   abs_path fast_abs_path
                   realpath fast_realpath
                  );

foreach my $func (@Functions) {
    no strict 'refs';
    my $cwd;
    eval { $cwd = &{'Cwd::'.$func} };
    is( $@, '',		"$func() should not explode under taint mode" );
    ok( tainted($cwd),	"its return value should be tainted" );
}

# Previous versions of Cwd tainted $^O
is !tainted($^O), 1, "\$^O should not be tainted";
