#!./perl

# There are few filetest operators that are portable enough to test.
# See pod/perlport.pod for details.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use Config;
plan(tests => 10);

ok( -d 'op' );
ok( -f 'TEST' );
ok( !-f 'op' );
ok( !-d 'TEST' );
ok( -r 'TEST' );

# make sure TEST is r-x
eval { chmod 0555, 'TEST' or die "chmod 0555, 'TEST' failed: $!" };
chomp ($bad_chmod = $@);

$oldeuid = $>;		# root can read and write anything
eval '$> = 1';		# so switch uid (may not be implemented)

print "# oldeuid = $oldeuid, euid = $>\n";

SKIP: {
    if (!$Config{d_seteuid}) {
	skip('no seteuid');
    } 
    elsif ($Config{config_args} =~/Dmksymlinks/) {
	skip('we cannot chmod symlinks');
    }
    elsif ($bad_chmod) {
	skip( $bad_chmod );
    }
    else {
	ok( !-w 'TEST' );
    }
}

# Scripts are not -x everywhere so cannot test that.

eval '$> = $oldeuid';	# switch uid back (may not be implemented)

# this would fail for the euid 1
# (unless we have unpacked the source code as uid 1...)
ok( -r 'op' );

# this would fail for the euid 1
# (unless we have unpacked the source code as uid 1...)
SKIP: {
    if ($Config{d_seteuid}) {
	ok( -w 'op' );
    } else {
	skip('no seteuid');
    }
}

ok( -x 'op' ); # Hohum.  Are directories -x everywhere?

is( "@{[grep -r, qw(foo io noo op zoo)]}", "io op" );
