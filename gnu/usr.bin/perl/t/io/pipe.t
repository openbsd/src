#!./perl

# $RCSfile: pipe.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:31 $

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    unless ($Config{'d_fork'}) {
	print "1..0\n";
	exit 0;
    }
}

$| = 1;
print "1..10\n";

open(PIPE, "|-") || (exec 'tr', 'YX', 'ko');
print PIPE "Xk 1\n";
print PIPE "oY 2\n";
close PIPE;

if (open(PIPE, "-|")) {
    while(<PIPE>) {
	s/^not //;
	print;
    }
}
else {
    print STDOUT "not ok 3\n";
    exec 'echo', 'not ok 4';
}

pipe(READER,WRITER) || die "Can't open pipe";

if ($pid = fork) {
    close WRITER;
    while(<READER>) {
	s/^not //;
	y/A-Z/a-z/;
	print;
    }
}
else {
    die "Couldn't fork" unless defined $pid;
    close READER;
    print WRITER "not ok 5\n";
    open(STDOUT,">&WRITER") || die "Can't dup WRITER to STDOUT";
    close WRITER;
    exec 'echo', 'not ok 6';
}


pipe(READER,WRITER) || die "Can't open pipe";
close READER;

$SIG{'PIPE'} = 'broken_pipe';

sub broken_pipe {
    print "ok 7\n";
}

print WRITER "not ok 7\n";
close WRITER;

print "ok 8\n";

# VMS doesn't like spawning subprocesses that are still connected to
# STDOUT.  Someone should modify tests #9 and #10 to work with VMS.

if ($^O eq 'VMS') {
    print "ok 9\n";
    print "ok 10\n";
    exit;
}

if ($Config{d_sfio} || $^O eq machten) {
    # Sfio doesn't report failure when closing a broken pipe
    # that has pending output.  Go figure.  MachTen doesn't either,
    # but won't write to broken pipes, so nothing's pending at close.
    print "ok 9\n";
}
else {
    local $SIG{PIPE} = 'IGNORE';
    open NIL, '|true'	or die "open failed: $!";
    sleep 2;
    print NIL 'foo'	or die "print failed: $!";
    if (close NIL) {
	print "not ok 9\n";
    }
    else {
	print "ok 9\n";
    }
}

# check that errno gets forced to 0 if the piped program exited non-zero
open NIL, '|exit 23;' or die "fork failed: $!";
$! = 1;
if (close NIL) {
    print "not ok 10\n# successful close\n";
}
elsif ($! != 0) {
    print "not ok 10\n# errno $!\n";
}
elsif ($? == 0) {
    print "not ok 10\n# status 0\n";
}
else {
    print "ok 10\n";
}
