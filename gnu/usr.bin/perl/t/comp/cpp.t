#!./perl

# $RCSfile: cpp.t,v $$Revision: 1.1.1.1 $$Date: 1996/08/19 10:13:11 $

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Config;
if ( ($Config{'cppstdin'} =~ /\bcppstdin\b/) and
     ( ! -x $Config{'scriptdir'} . "/cppstdin") ) {
    print "1..0\n";
    exit; 		# Cannot test till after install, alas.
}

system "./perl -P comp/cpp.aux"
