#!perl

BEGIN {
  chdir 't' if -d 't';
  @INC = '../lib' if -d '../lib' && -d '../ext';

  require "./test.pl";
  require Config; import Config;

  if ($ENV{'PERL_CORE'} && $Config{'extensions'} !~ m[\bIPC/SysV\b]) {
    skip_all('-- IPC::SysV was not built');
  }
  skip_all_if_miniperl();
  if ($Config{'d_sem'} ne 'define') {
    skip_all('-- $Config{d_sem} undefined');
  }
}

use strict;
our $TODO;

use sigtrap qw/die normal-signals error-signals/;
use IPC::SysV qw/ IPC_PRIVATE S_IRUSR S_IWUSR IPC_RMID SETVAL GETVAL SETALL GETALL IPC_CREAT /;

my $id;
my $nsem = 10;
END { semctl $id, 0, IPC_RMID, 0 if defined $id }

{
    local $SIG{SYS} = sub { skip_all("SIGSYS caught") } if exists $SIG{SYS};
    $id = semget IPC_PRIVATE, $nsem, S_IRUSR | S_IWUSR | IPC_CREAT;
}

if (not defined $id) {
    my $info = "semget failed: $!";
    if ($! == &IPC::SysV::ENOSPC || $! == &IPC::SysV::ENOSYS ||
	$! == &IPC::SysV::ENOMEM || $! == &IPC::SysV::EACCES) {
        skip_all($info);
    }
    else {
        die $info;
    }
}
else {
    plan(tests => 7);
    pass('acquired semaphore');
}

{ # [perl #120635] 64 bit big-endian semctl SETVAL bug
    ok(semctl($id, "ignore", SETALL, pack("s!*",(0)x$nsem)),
       "Initialize all $nsem semaphores to zero");

    my $sem2set = 3;
    my $semval = 17;
    ok(semctl($id, $sem2set, SETVAL, $semval),
       "Set semaphore $sem2set to $semval");

    my $semvals;
    ok(semctl($id, "ignore", GETALL, $semvals),
       'Get current semaphore values');

    my @semvals = unpack("s!*", $semvals);
    is(scalar(@semvals), $nsem, 
       "Make sure we get back statuses for all $nsem semaphores");

    is($semvals[$sem2set], $semval, 
       "Checking value of semaphore $sem2set");

    is(semctl($id, $sem2set, GETVAL, "ignored"), $semval,
       "Check value via GETVAL");
}

