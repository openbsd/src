BEGIN {
    chdir 't' if -d 't';

    @INC = qw(. ../lib);
    require 'test.pl';
}

require Config; import Config;

$TEST_COUNT = 11;

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
    plan( tests => $TEST_COUNT );
}

use IPC::SysV qw(
	SETALL
	IPC_PRIVATE
	IPC_CREAT
	IPC_RMID
	IPC_NOWAIT
	IPC_STAT
	S_IRWXU
	S_IRWXG
	S_IRWXO
);
use IPC::Semaphore;

SKIP: {

my $sem =
    IPC::Semaphore->new(IPC_PRIVATE, 10, S_IRWXU | S_IRWXG | S_IRWXO | IPC_CREAT);
if (!$sem) {
    if ($! eq 'No space left on device') {
        # "normal" error
        skip( "cannot proceed: IPC::Semaphore->new() said: $!", $TEST_COUNT);
    }
    else {
        # unexpected error
        die "IPC::Semaphore->new(): ",$!+0," $!\n";
    }
}

pass('acquired a semaphore');

ok(my $st = $sem->stat,'stat it');

ok($sem->setall( (0) x 10),'set all');

my @sem = $sem->getall;
cmp_ok(join("",@sem),'eq',"0000000000",'get all');

$sem[2] = 1;
ok($sem->setall( @sem ),'set after change');

@sem = $sem->getall;
cmp_ok(join("",@sem),'eq',"0010000000",'get again');

my $ncnt = $sem->getncnt(0);
ok(!$sem->getncnt(0),'procs waiting now');
ok(defined($ncnt),'prev procs waiting');

ok($sem->op(2,-1,IPC_NOWAIT),'op nowait');

ok(!$sem->getncnt(0),'no procs waiting');

END {
    if ($sem) {
        ok($sem->remove,'release');
    }
}

} # SKIP
