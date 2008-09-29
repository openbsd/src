#!./perl

BEGIN {
	chdir 't' if -d 't';
	unshift @INC, '../lib';
}

BEGIN{
	# Don't do anything if POSIX is missing, or sigaction missing.
	use Config;
	eval 'use POSIX';
	if($@ || $^O eq 'MSWin32' || $^O eq 'NetWare' || $^O eq 'dos' ||
	   $^O eq 'MacOS' || ($^O eq 'VMS' && !$Config{'d_sigaction'})) {
		print "1..0\n";
		exit 0;
	}
}

use Test::More tests => 31;

use strict;
use vars qw/$bad $bad7 $ok10 $bad18 $ok/;

$^W=1;

sub IGNORE {
	$bad7=1;
}

sub DEFAULT {
	$bad18=1;
}

sub foo {
	$ok=1;
}

my $newaction=POSIX::SigAction->new('::foo', new POSIX::SigSet(SIGUSR1), 0);
my $oldaction=POSIX::SigAction->new('::bar', new POSIX::SigSet(), 0);

{
	my $bad;
	local($SIG{__WARN__})=sub { $bad=1; };
	sigaction(SIGHUP, $newaction, $oldaction);
	ok(!$bad, "no warnings");
}

ok($oldaction->{HANDLER} eq 'DEFAULT' ||
   $oldaction->{HANDLER} eq 'IGNORE', $oldaction->{HANDLER});

is($SIG{HUP}, '::foo');

sigaction(SIGHUP, $newaction, $oldaction);
is($oldaction->{HANDLER}, '::foo');

ok($oldaction->{MASK}->ismember(SIGUSR1), "SIGUSR1 ismember MASK");

SKIP: {
    skip("sigaction() thinks different in $^O", 1)
	if $^O eq 'linux' || $^O eq 'unicos';
    is($oldaction->{FLAGS}, 0);
}

$newaction=POSIX::SigAction->new('IGNORE');
sigaction(SIGHUP, $newaction);
kill 'HUP', $$;
ok(!$bad, "SIGHUP ignored");

is($SIG{HUP}, 'IGNORE');
sigaction(SIGHUP, POSIX::SigAction->new('DEFAULT'));
is($SIG{HUP}, 'DEFAULT');

$newaction=POSIX::SigAction->new(sub { $ok10=1; });
sigaction(SIGHUP, $newaction);
{
	local($^W)=0;
	kill 'HUP', $$;
}
ok($ok10, "SIGHUP handler called");

is(ref($SIG{HUP}), 'CODE');

sigaction(SIGHUP, POSIX::SigAction->new('::foo'));
# Make sure the signal mask gets restored after sigaction croak()s.
eval {
	my $act=POSIX::SigAction->new('::foo');
	delete $act->{HANDLER};
	sigaction(SIGINT, $act);
};
kill 'HUP', $$;
ok($ok, "signal mask gets restored after croak");

undef $ok;
# Make sure the signal mask gets restored after sigaction returns early.
my $x=defined sigaction(SIGKILL, $newaction, $oldaction);
kill 'HUP', $$;
ok(!$x && $ok, "signal mask gets restored after early return");

$SIG{HUP}=sub {};
sigaction(SIGHUP, $newaction, $oldaction);
is(ref($oldaction->{HANDLER}), 'CODE');

eval {
	sigaction(SIGHUP, undef, $oldaction);
};
ok(!$@, "undef for new action");

eval {
	sigaction(SIGHUP, 0, $oldaction);
};
ok(!$@, "zero for new action");

eval {
	sigaction(SIGHUP, bless({},'Class'), $oldaction);
};
ok($@, "any object not good as new action");

SKIP: {
    skip("SIGCONT not trappable in $^O", 1)
	if ($^O eq 'VMS');
    $newaction=POSIX::SigAction->new(sub { $ok10=1; });
    if (eval { SIGCONT; 1 }) {
	sigaction(SIGCONT, POSIX::SigAction->new('DEFAULT'));
	{
	    local($^W)=0;
	    kill 'CONT', $$;
	}
    }
    ok(!$bad18, "SIGCONT trappable");
}

{
    local $SIG{__WARN__} = sub { }; # Just suffer silently.

    my $hup20;
    my $hup21;

    sub hup20 { $hup20++ }
    sub hup21 { $hup21++ }

    sigaction("FOOBAR", $newaction);
    ok(1, "no coredump, still alive");

    $newaction = POSIX::SigAction->new("hup20");
    sigaction("SIGHUP", $newaction);
    kill "HUP", $$;
    is($hup20, 1);

    $newaction = POSIX::SigAction->new("hup21");
    sigaction("HUP", $newaction);
    kill "HUP", $$;
    is ($hup21, 1);
}

# "safe" attribute.
# for this one, use the accessor instead of the attribute

# standard signal handling via %SIG is safe
$SIG{HUP} = \&foo;
$oldaction = POSIX::SigAction->new;
sigaction(SIGHUP, undef, $oldaction);
ok($oldaction->safe, "SIGHUP is safe");

# SigAction handling is not safe ...
sigaction(SIGHUP, POSIX::SigAction->new(\&foo));
sigaction(SIGHUP, undef, $oldaction);
ok(!$oldaction->safe, "SigAction not safe by default");

# ... unless we say so!
$newaction = POSIX::SigAction->new(\&foo);
$newaction->safe(1);
sigaction(SIGHUP, $newaction);
sigaction(SIGHUP, undef, $oldaction);
ok($oldaction->safe, "SigAction can be safe");

# And safe signal delivery must work
$ok = 0;
kill 'HUP', $$;
ok($ok, "safe signal delivery must work");

SKIP: {
    eval 'use POSIX qw(%SIGRT SIGRTMIN SIGRTMAX); scalar %SIGRT + SIGRTMIN() + SIGRTMAX()';
    $@					# POSIX did not exort
    || SIGRTMIN() < 0 || SIGRTMAX() < 0	# HP-UX 10.20 exports both as -1
    || SIGRTMIN() > $Config{sig_count}	# AIX 4.3.3 exports bogus 888 and 999
	and skip("no SIGRT signals", 4);
    ok(SIGRTMAX() > SIGRTMIN(), "SIGRTMAX > SIGRTMIN");
    is(scalar %SIGRT, SIGRTMAX() - SIGRTMIN() + 1, "scalar SIGRT");
    my $sigrtmin;
    my $h = sub { $sigrtmin = 1 };
    $SIGRT{SIGRTMIN} = $h;
    is($SIGRT{SIGRTMIN}, $h, "handler set & get");
    kill 'SIGRTMIN', $$;
    is($sigrtmin, 1, "SIGRTMIN handler works");
}

SKIP: {
    eval 'use POSIX qw(SA_SIGINFO); SA_SIGINFO';
    skip("no SA_SIGINFO", 1) if $@;
    sub hiphup {
	is($_[1]->{signo}, SIGHUP, "SA_SIGINFO got right signal");
    }
    my $act = POSIX::SigAction->new('hiphup', 0, SA_SIGINFO);
    sigaction(SIGHUP, $act);
    kill 'HUP', $$;
}

eval { sigaction(-999, "foo"); };
like($@, qr/Negative signals/,
    "Prevent negative signals instead of core dumping");
