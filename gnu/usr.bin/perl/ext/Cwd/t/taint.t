#!./perl -Tw
# Testing Cwd under taint mode.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Cwd;
use Test::More tests => 6;
use Scalar::Util qw/tainted/;

my $cwd;
eval { $cwd = getcwd; };
is( $@, '',		'getcwd() does not explode under taint mode' );
ok( tainted($cwd),	"its return value is tainted" );

eval { $cwd = cwd; };
is( $@, '',		'cwd() does not explode under taint mode' );
ok( tainted($cwd),	"its return value is tainted" );

eval { $cwd = fastcwd; };
is( $@, '',		'fastcwd() does not explode under taint mode' );
ok( tainted($cwd),	"its return value is tainted" );
