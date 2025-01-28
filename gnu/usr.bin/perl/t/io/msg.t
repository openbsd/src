#!perl

BEGIN {
  chdir 't' if -d 't';

  require "./test.pl";
  set_up_inc( '../lib' ) if -d '../lib' && -d '../ext';
  require Config; Config->import;

  if ($ENV{'PERL_CORE'} && $Config{'extensions'} !~ m[\bIPC/SysV\b]) {
    skip_all('-- IPC::SysV was not built');
  }
  skip_all_if_miniperl();
  if ($Config{'d_msg'} ne 'define') {
    skip_all('-- $Config{d_msg} undefined');
  }
}

use strict;
use warnings;
our $TODO;

use sigtrap qw/die normal-signals error-signals/;
use IPC::SysV qw/ IPC_PRIVATE S_IRUSR S_IWUSR IPC_RMID IPC_CREAT IPC_STAT IPC_CREAT IPC_NOWAIT/;
use Errno qw(EINVAL);

my $id;
END { msgctl $id, IPC_RMID, 0 if defined $id }

{
    local $SIG{SYS} = sub { skip_all("SIGSYS caught") } if exists $SIG{SYS};
    $id = msgget IPC_PRIVATE, S_IRUSR | S_IWUSR | IPC_CREAT;
}

if (not defined $id) {
    my $info = "msgget failed: $!";
    if ($! == &IPC::SysV::ENOSPC || $! == &IPC::SysV::ENOSYS ||
	$! == &IPC::SysV::ENOMEM || $! == &IPC::SysV::EACCES) {
        skip_all($info);
    }
    else {
        die $info;
    }
}
else {
    pass('acquired msg queue');
}

{
    # basic send/receive
    my $type = 0x1F0;
    my $content = "AB\xFF\xC0";

    my $msg = pack("l! a*", $type, $content);
    if (ok(msgsnd($id, $msg, IPC_NOWAIT), "send a message")) {
        my $rcvbuf;
        ok(msgrcv($id, $rcvbuf, 1024, 0, IPC_NOWAIT), "receive it");
        is($rcvbuf, $msg, "received should match sent");
    }

    # try upgraded send
    utf8::upgrade(my $umsg = $msg);
    if (ok(msgsnd($id, $umsg, IPC_NOWAIT), "send a message (upgraded)")) {
        my $rcvbuf;
        ok(msgrcv($id, $rcvbuf, 1024, 0, IPC_NOWAIT), "receive it");
        is($rcvbuf, $msg, "received should match sent");
    }

    # try a receive buffer that starts upgraded
    if (ok(msgsnd($id, $msg, IPC_NOWAIT), "send a message (upgraded receiver)")) {
        my $rcvbuf = "\x{101}";
        ok(msgrcv($id, $rcvbuf, 1024, 0, IPC_NOWAIT), "receive it (upgraded receiver)");
        is($rcvbuf, $msg, "received should match sent (upgraded receiver)");
    }
}

{
    # receive to magic
    my $x;
    my $fetch = 0;
    my $store = 0;
    package MyScalar {
        sub TIESCALAR { bless {}, shift }
        sub FETCH { ++$fetch; $x }
        sub STORE { ++$store; $x = $_[1]; }
    };
    tie my $rcvbuf, "MyScalar";
    my $msg = pack("l! a*", 1, "Hello");
    my $warn = "";
    if (ok(msgsnd($id, $msg, IPC_NOWAIT), "send to magic receive")) {
        {
            local $SIG{__WARN__} = sub { $warn .= "@_\n" };
            ok(msgrcv($id, $rcvbuf, 1024, 0, IPC_NOWAIT), "receive it (magic receiver)");
        }
        is($x, $msg, "magic properly triggered");
        is($fetch, 0, "should be no fetch");
        is($store, 1, "should be one store");
        unlike($warn, qr/uninitialized/, "shouldn't be uninitialized warning");
    }
}

{
    # this resulted in a panic
    my $buf;
    ok(!msgrcv($id, $buf, -10, 0, IPC_NOWAIT), "fail with negative length");
    is(0+$!, &Errno::EINVAL, "check proper error");
}

done_testing();
