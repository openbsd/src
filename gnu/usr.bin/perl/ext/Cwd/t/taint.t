#!./perl -Tw
# Testing Cwd under taint mode.

use Cwd;
BEGIN {
    chdir 't' if -d 't';
}

use strict;
use Test::More tests => 10;
use Scalar::Util qw/tainted/;

my @Functions = qw(getcwd cwd fastcwd
                   abs_path fast_abs_path
                  );

foreach my $func (@Functions) {
    no strict 'refs';
    my $cwd;
    eval { $cwd = &{'Cwd::'.$func} };
    is( $@, '',		"$func() should not explode under taint mode" );
    ok( tainted($cwd),	"its return value should be tainted" );
}
