#!./perl -Tw
# Testing Cwd under taint mode.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use strict;
use Cwd;
use Test::More tests => 10;
use Scalar::Util qw/tainted/;

my @Functions = qw(getcwd cwd fastcwd
                   abs_path fast_abs_path
                  );

foreach my $func (@Functions) {
    no strict 'refs';
    my $cwd;
    eval { $cwd = &{'Cwd::'.$func} };
    is( $@, '',		"$func() does not explode under taint mode" );
    ok( tainted($cwd),	"its return value is tainted" );
}
