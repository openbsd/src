# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..3\n"; }
END {print "not ok 1\n" unless $loaded;}
use JNI;
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):

# Simple StringBuffer test.
#
use JPL::AutoLoader;
use JPL::Class 'java::lang::StringBuffer';
$sb = java::lang::StringBuffer->new__s("TEST");
if ($sb->toString____s() eq "TEST") {
    print "ok 2\n";
} else {
    print "not ok 2\n";
}

# Put up a frame and let the user close it.
#
use JPL::AutoLoader;
use JPL::Class 'java::awt::Frame';
use JPL::Class 'Closer';

$f = java::awt::Frame->new__s("Close Me, Please!");
my $setSize = getmeth("setSize", ["int", "int"], []);
my $addWindowListener = getmeth("addWindowListener",
            ["java.awt.event.WindowListener"], []);

$f->$addWindowListener( new Closer );
$f->$setSize(200,200);
$f->show();

while (1) {

    if (!$f->isVisible____Z) {
        last;
    }

    # Sleep a bit.
    #
    sleep 1;
}

print "ok 3\n";
