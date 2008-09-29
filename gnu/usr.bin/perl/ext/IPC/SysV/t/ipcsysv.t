BEGIN {
    chdir 't' if -d 't';

    @INC = qw(. ../lib);

    require Config; import Config;
    require 'test.pl';
}

if ($Config{'extensions'} !~ /\bIPC\/SysV\b/) {
    skip_all('IPC::SysV was not built');
}
elsif ($Config{'d_sem'} ne 'define') {
    skip_all('$Config{d_sem} undefined');
}
elsif ($Config{'d_msg'} ne 'define') {
    skip_all('$Config{d_msg} undefined');
}
else {
    plan( tests => 17 );
}

# These constants are common to all tests.
# Later the sem* tests will import more for themselves.

use IPC::SysV qw(IPC_PRIVATE IPC_NOWAIT IPC_STAT IPC_RMID S_IRWXU);
use strict;

my $msg;
my $sem;

# FreeBSD is known to throw this if there's no SysV IPC in the kernel.
$SIG{SYS} = sub {
    diag(<<EOM);
SIGSYS caught.
It may be that your kernel does not have SysV IPC configured.

EOM
    if ($^O eq 'freebsd') {
        diag(<<EOM);
You must have following options in your kernel:

options         SYSVSHM
options         SYSVSEM
options         SYSVMSG

See config(8).

EOM
    }
    diag('Bail out! SIGSYS caught');
    exit(1);
};

my $perm = S_IRWXU;

SKIP: {

skip( 'lacking d_msgget d_msgctl d_msgsnd d_msgrcv', 6 ) unless
    $Config{'d_msgget'} eq 'define' &&
    $Config{'d_msgctl'} eq 'define' &&
    $Config{'d_msgsnd'} eq 'define' &&
    $Config{'d_msgrcv'} eq 'define';

    $msg = msgget(IPC_PRIVATE, $perm);
    # Very first time called after machine is booted value may be 0 
    if (!(defined($msg) && $msg >= 0)) {
        skip( "msgget failed: $!", 6);
    }
    else {
        pass('msgget IPC_PRIVATE S_IRWXU');
    }

    #Putting a message on the queue
    my $msgtype = 1;
    my $msgtext = "hello";

    my $test2bad;
    my $test5bad;
    my $test6bad;

    my $test_name = 'queue a message';
    if (msgsnd($msg,pack("L! a*",$msgtype,$msgtext),IPC_NOWAIT)) {
        pass($test_name);
    }
    else {
        fail($test_name);
        $test2bad = 1;
        diag(<<EOM);
The failure of the subtest #2 may indicate that the message queue
resource limits either of the system or of the testing account
have been reached.  Error message "Operating would block" is
usually indicative of this situation.  The error message was now:
"$!"

You can check the message queues with the 'ipcs' command and
you can remove unneeded queues with the 'ipcrm -q id' command.
You may also consider configuring your system or account
to have more message queue resources.

Because of the subtest #2 failing also the substests #5 and #6 will
very probably also fail.
EOM
    }

    my $data;
    ok(msgctl($msg,IPC_STAT,$data),'msgctl IPC_STAT call');

    cmp_ok(length($data),'>',0,'msgctl IPC_STAT data');

    my $test_name = 'message get call';
    my $msgbuf;
    if (msgrcv($msg,$msgbuf,256,0,IPC_NOWAIT)) {
        pass($test_name);
    }
    else {
        fail($test_name);
        $test5bad = 1;
    }
    if ($test5bad && $test2bad) {
        diag(<<EOM);
This failure was to be expected because the subtest #2 failed.
EOM
    }

    my $test_name = 'message get data';
    my($rmsgtype,$rmsgtext);
    ($rmsgtype,$rmsgtext) = unpack("L! a*",$msgbuf);
    if ($rmsgtype == $msgtype && $rmsgtext eq $msgtext) {
        pass($test_name);
    }
    else {
        fail($test_name);
        $test6bad = 1;
    }
    if ($test6bad && $test2bad) {
    print <<EOM;
This failure was to be expected because the subtest #2 failed.
EOM
     }
} # SKIP

SKIP: {

    skip('lacking d_semget d_semctl', 11) unless
        $Config{'d_semget'} eq 'define' &&
        $Config{'d_semctl'} eq 'define';

    use IPC::SysV qw(IPC_CREAT GETALL SETALL);

    # FreeBSD's default limit seems to be 9
    my $nsem = 5;

    my $test_name = 'sem acquire';
    $sem = semget(IPC_PRIVATE, $nsem, $perm | IPC_CREAT);
    if ($sem) {
        pass($test_name);
    }
    else {
        diag("cannot proceed: semget() error: $!");
        skip('semget() resource unavailable', 11)
            if $! eq 'No space left on device';

        # Very first time called after machine is booted value may be 0 
        die "semget: $!\n" unless defined($sem) && $sem >= 0;
    }

    my $data;
    ok(semctl($sem,0,IPC_STAT,$data),'sem data call');

    cmp_ok(length($data),'>',0,'sem data len');

    ok(semctl($sem,0,SETALL,pack("s!*",(0) x $nsem)), 'set all sems');

    $data = "";
    ok(semctl($sem,0,GETALL,$data), 'get all sems');

    is(length($data),length(pack("s!*",(0) x $nsem)), 'right length');

    my @data = unpack("s!*",$data);

    my $adata = "0" x $nsem;

    is(scalar(@data),$nsem,'right amount');
    cmp_ok(join("",@data),'eq',$adata,'right data');

    my $poke = 2;

    $data[$poke] = 1;
    ok(semctl($sem,0,SETALL,pack("s!*",@data)),'poke it');
    
    $data = "";
    ok(semctl($sem,0,GETALL,$data),'and get it back');

    @data = unpack("s!*",$data);
    my $bdata = "0" x $poke . "1" . "0" x ($nsem-$poke-1);

    cmp_ok(join("",@data),'eq',$bdata,'changed');
} # SKIP

END {
    msgctl($msg,IPC_RMID,0)       if defined $msg;
    semctl($sem,0,IPC_RMID,undef) if defined $sem;
}
