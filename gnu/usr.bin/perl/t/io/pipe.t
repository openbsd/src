#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    require './test.pl';

    if (!$Config{'d_fork'}) {
        skip_all("fork required to pipe");
    }
    else {
        plan(tests => 22);
    }
}

my $Perl = which_perl();


$| = 1;

open(PIPE, "|-") || exec $Perl, '-pe', 'tr/YX/ko/';

printf PIPE "Xk %d - open |- || exec\n", curr_test();
next_test();
printf PIPE "oY %d -    again\n", curr_test();
next_test();
close PIPE;

SKIP: {
    # Technically this should be TODO.  Someone try it if you happen to
    # have a vmesa machine.
    skip "Doesn't work here yet", 4 if $^O eq 'vmesa';

    if (open(PIPE, "-|")) {
	while(<PIPE>) {
	    s/^not //;
	    print;
	}
	close PIPE;        # avoid zombies
    }
    else {
	printf STDOUT "not ok %d - open -|\n", curr_test();
        next_test();
        my $tnum = curr_test;
        next_test();
	exec $Perl, '-le', "print q{not ok $tnum -     again}";
    }

    # This has to be *outside* the fork
    next_test() for 1..2;

    SKIP: {
        skip "fork required", 2 unless $Config{d_fork};

        pipe(READER,WRITER) || die "Can't open pipe";

        if ($pid = fork) {
            close WRITER;
            while(<READER>) {
                s/^not //;
                y/A-Z/a-z/;
                print;
            }
            close READER;     # avoid zombies
        }
        else {
            die "Couldn't fork" unless defined $pid;
            close READER;
            printf WRITER "not ok %d - pipe & fork\n", curr_test;
            next_test;

            open(STDOUT,">&WRITER") || die "Can't dup WRITER to STDOUT";
            close WRITER;
            
            my $tnum = curr_test;
            next_test;
            exec $Perl, '-le', "print q{not ok $tnum -     with fh dup }";
        }

        # This has to be done *outside* the fork.
        next_test() for 1..2;
    }
} 
wait;				# Collect from $pid

pipe(READER,WRITER) || die "Can't open pipe";
close READER;

$SIG{'PIPE'} = 'broken_pipe';

sub broken_pipe {
    $SIG{'PIPE'} = 'IGNORE';       # loop preventer
    printf "ok %d - SIGPIPE\n", curr_test;
}

printf WRITER "not ok %d - SIGPIPE\n", curr_test;
close WRITER;
sleep 1;
next_test;
pass();

# VMS doesn't like spawning subprocesses that are still connected to
# STDOUT.  Someone should modify these tests to work with VMS.

SKIP: {
    skip "doesn't like spawning subprocesses that are still connected", 10
      if $^O eq 'VMS';

    SKIP: {
        # Sfio doesn't report failure when closing a broken pipe
        # that has pending output.  Go figure.  MachTen doesn't either,
        # but won't write to broken pipes, so nothing's pending at close.
        # BeOS will not write to broken pipes, either.
        # Nor does POSIX-BC.
        skip "Won't report failure on broken pipe", 1
          if $Config{d_sfio} || $^O eq 'machten' || $^O eq 'beos' || 
             $^O eq 'posix-bc';

        local $SIG{PIPE} = 'IGNORE';
        open NIL, qq{|$Perl -e "exit 0"} or die "open failed: $!";
        sleep 5;
        if (print NIL 'foo') {
            # If print was allowed we had better get an error on close
            ok( !close NIL,     'close error on broken pipe' );
        }
        else {
            ok(close NIL,       'print failed on broken pipe');
        }
    }

    SKIP: {
        skip "Don't work yet", 9 if $^O eq 'vmesa';

        # check that errno gets forced to 0 if the piped program exited 
        # non-zero
        open NIL, qq{|$Perl -e "exit 23";} or die "fork failed: $!";
        $! = 1;
        ok(!close NIL,  'close failure on non-zero piped exit');
        is($!, '',      '       errno');
        isnt($?, 0,     '       status');

        SKIP: {
            skip "Don't work yet", 6 if $^O eq 'mpeix';

            # check that status for the correct process is collected
            my $zombie;
            unless( $zombie = fork ) {
                $NO_ENDING=1;
                exit 37;
            }
            my $pipe = open *FH, "sleep 2;exit 13|" or die "Open: $!\n";
            $SIG{ALRM} = sub { return };
            alarm(1);
            is( close FH, '',   'close failure for... umm, something' );
            is( $?, 13*256,     '       status' );
            is( $!, '',         '       errno');

            my $wait = wait;
            is( $?, 37*256,     'status correct after wait' );
            is( $wait, $zombie, '       wait pid' );
            is( $!, '',         '       errno');
        }
    }
}

# Test new semantics for missing command in piped open
# 19990114 M-J. Dominus mjd@plover.com
{ local *P;
  ok( !open(P, "|    "),        'missing command in piped open input' );
  ok( !open(P, "     |"),       '                              output');
}

# check that status is unaffected by implicit close
{
    local(*NIL);
    open NIL, qq{|$Perl -e "exit 23"} or die "fork failed: $!";
    $? = 42;
    # NIL implicitly closed here
}
is($?, 42,      'status unaffected by implicit close');
$? = 0;

# check that child is reaped if the piped program can't be executed
{
  open NIL, '/no_such_process |';
  close NIL;

  my $child = 0;
  eval {
    local $SIG{ALRM} = sub { die; };
    alarm 2;
    $child = wait;
    alarm 0;
  };

  is($child, -1, 'child reaped if piped program cannot be executed');
}
