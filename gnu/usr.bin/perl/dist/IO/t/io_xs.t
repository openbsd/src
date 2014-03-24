#!./perl

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

use Test::More tests => 5;
use IO::File;
use IO::Seekable;

$x = new_tmpfile IO::File;
ok($x, "new_tmpfile");
print $x "ok 2\n";
$x->seek(0,SEEK_SET);
my $line = <$x>;
is($line, "ok 2\n", "check we can write to the tempfile");

$x->seek(0,SEEK_SET);
print $x "not ok 3\n";
$p = $x->getpos;
print $x "ok 3\n";
$x->flush;
$x->setpos($p);
$line = <$x>;
is($line, "ok 3\n", "test getpos/setpos");

$! = 0;
$x->setpos(undef);
ok($!, "setpos(undef) makes errno non-zero");

SKIP:
{ # [perl #64772] IO::Handle->sync fails on an O_RDONLY descriptor
    $Config{d_fsync}
       or skip "No fsync", 1;
    $^O eq 'aix'
      and skip "fsync() documented to fail on non-writable handles on AIX", 1;
    $^O eq 'cygwin'
      and skip "fsync() on cygwin uses FlushFileBuffers which requires a writable handle", 1;
    open my $fh, "<", "t/io_xs.t"
       or skip "Cannot open t/io_xs.t read-only: $!", 1;
    ok($fh->sync, "sync to a read only handle")
	or diag "sync(): ", $!;
}
