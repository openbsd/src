#!./perl

# $RCSfile: cpp.t,v $$Revision: 1.6 $$Date: 2002/10/27 22:25:35 $

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    $ENV{PERL5LIB} = '../lib';
}

use Config;
if ( $^O eq 'MacOS' ||
     ($Config{'cppstdin'} =~ /\bcppstdin\b/) &&
     ! -x $Config{'binexp'} . "/cppstdin" ) {
    print "1..0 # Skip: \$Config{cppstdin} unavailable\n";
    exit; 		# Cannot test till after install, alas.
}

system qq{$^X -"P" "comp/cpp.aux"};
