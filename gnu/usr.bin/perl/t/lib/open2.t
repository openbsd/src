#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    unless ($Config{'d_fork'}) {
	print "1..0\n";
	exit 0;
    }
    # make warnings fatal
    $SIG{__WARN__} = sub { die @_ };
}

use strict;
use IO::Handle;
use IPC::Open2;
#require 'open2.pl'; use subs 'open2';

my $perl = './perl';

sub ok {
    my ($n, $result, $info) = @_;
    if ($result) {
	print "ok $n\n";
    }
    else {
    	print "not ok $n\n";
	print "# $info\n" if $info;
    }
}

my ($pid, $reaped_pid);
STDOUT->autoflush;
STDERR->autoflush;

print "1..7\n";

ok 1, $pid = open2 'READ', 'WRITE', $perl, '-e', 'print scalar <STDIN>';
ok 2, print WRITE "hi kid\n";
ok 3, <READ> eq "hi kid\n";
ok 4, close(WRITE), $!;
ok 5, close(READ), $!;
$reaped_pid = waitpid $pid, 0;
ok 6, $reaped_pid == $pid, $reaped_pid;
ok 7, $? == 0, $?;
