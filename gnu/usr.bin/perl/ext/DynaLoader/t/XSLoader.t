#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bSDBM_File\b/) {
	print "1..0 # Skip: no SDBM_File\n";
	exit 0;
    }
}

use Test;
plan tests => 4;

use XSLoader;
ok(1);
ok( ref XSLoader->can('load') );

eval { XSLoader::load(); };
ok( $@ =~ /^XSLoader::load\('Your::Module', \$Your::Module::VERSION\)/ );

package SDBM_File;
XSLoader::load('SDBM_File');
::ok( ref SDBM_File->can('TIEHASH') );
