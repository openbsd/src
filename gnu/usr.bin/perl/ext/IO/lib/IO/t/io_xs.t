#!./perl

BEGIN {
    unless(grep /blib/, @INC) {
	chdir 't' if -d 't';
	@INC = '../lib';
    }
}

use Config;

BEGIN {
    if($ENV{PERL_CORE}) {
        if ($Config{'extensions'} !~ /\bIO\b/) {
	    print "1..0 # Skip: IO extension not built\n";
	    exit 0;
        }
    }
    if( $^O eq 'VMS' && $Config{'vms_cc_type'} ne 'decc' ) {
        print "1..0 # Skip: not compatible with the VAXCRTL\n";
        exit 0;
    }
}

use IO::File;
use IO::Seekable;

print "1..4\n";

$x = new_tmpfile IO::File or print "not ";
print "ok 1\n";
print $x "ok 2\n";
$x->seek(0,SEEK_SET);
print <$x>;

$x->seek(0,SEEK_SET);
print $x "not ok 3\n";
$p = $x->getpos;
print $x "ok 3\n";
$x->flush;
$x->setpos($p);
print scalar <$x>;

$! = 0;
$x->setpos(undef);
print $! ? "ok 4 # $!\n" : "not ok 4\n";

