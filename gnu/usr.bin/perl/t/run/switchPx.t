#!./perl

# Ensure that the -P and -x flags work together.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    $ENV{PERL5LIB} = '../lib';

    use Config;
    if ( $^O eq 'MacOS' || ($Config{'cppstdin'} =~ /\bcppstdin\b/) &&
	 ! -x $Config{'binexp'} . "/cppstdin" ) {
	print "1..0 # Skip: \$Config{cppstdin} unavailable\n";
	    exit; 		# Cannot test till after install, alas.
    }
}

require './test.pl';

print runperl( switches => ['-Px'], 
               nolib => 1,   # for some reason this is necessary under VMS
               progfile => 'run/switchPx.aux' );
