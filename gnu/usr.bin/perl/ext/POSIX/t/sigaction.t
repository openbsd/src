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

use strict;
use vars qw/$bad7 $ok10 $bad18 $ok/;

$^W=1;

print "1..25\n";

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
	if($bad) { print "not ok 1\n" } else { print "ok 1\n"}
}

if($oldaction->{HANDLER} eq 'DEFAULT' ||
   $oldaction->{HANDLER} eq 'IGNORE')
  { print "ok 2\n" } else { print "not ok 2 # ", $oldaction->{HANDLER}, "\n"}
print $SIG{HUP} eq '::foo' ? "ok 3\n" : "not ok 3\n";

sigaction(SIGHUP, $newaction, $oldaction);
if($oldaction->{HANDLER} eq '::foo')
  { print "ok 4\n" } else { print "not ok 4\n"}
if($oldaction->{MASK}->ismember(SIGUSR1))
  { print "ok 5\n" } else { print "not ok 5\n"}
if($oldaction->{FLAGS}) {
    if ($^O eq 'linux' || $^O eq 'unicos') {
	print "ok 6 # Skip: sigaction() thinks different in $^O\n";
    } else {
	print "not ok 6\n";
    }
} else {
    print "ok 6\n";
}

$newaction=POSIX::SigAction->new('IGNORE');
sigaction(SIGHUP, $newaction);
kill 'HUP', $$;
print $bad7 ? "not ok 7\n" : "ok 7\n";

print $SIG{HUP} eq 'IGNORE' ? "ok 8\n" : "not ok 8\n";
sigaction(SIGHUP, POSIX::SigAction->new('DEFAULT'));
print $SIG{HUP} eq 'DEFAULT' ? "ok 9\n" : "not ok 9\n";

$newaction=POSIX::SigAction->new(sub { $ok10=1; });
sigaction(SIGHUP, $newaction);
{
	local($^W)=0;
	kill 'HUP', $$;
}
print $ok10 ? "ok 10\n" : "not ok 10\n";

print ref($SIG{HUP}) eq 'CODE' ? "ok 11\n" : "not ok 11\n";

sigaction(SIGHUP, POSIX::SigAction->new('::foo'));
# Make sure the signal mask gets restored after sigaction croak()s.
eval {
	my $act=POSIX::SigAction->new('::foo');
	delete $act->{HANDLER};
	sigaction(SIGINT, $act);
};
kill 'HUP', $$;
print $ok ? "ok 12\n" : "not ok 12\n";

undef $ok;
# Make sure the signal mask gets restored after sigaction returns early.
my $x=defined sigaction(SIGKILL, $newaction, $oldaction);
kill 'HUP', $$;
print !$x && $ok ? "ok 13\n" : "not ok 13\n";

$SIG{HUP}=sub {};
sigaction(SIGHUP, $newaction, $oldaction);
print ref($oldaction->{HANDLER}) eq 'CODE' ? "ok 14\n" : "not ok 14\n";

eval {
	sigaction(SIGHUP, undef, $oldaction);
};
print $@ ? "not ok 15\n" : "ok 15\n";

eval {
	sigaction(SIGHUP, 0, $oldaction);
};
print $@ ? "not ok 16\n" : "ok 16\n";

eval {
	sigaction(SIGHUP, bless({},'Class'), $oldaction);
};
print $@ ? "ok 17\n" : "not ok 17\n";

if ($^O eq 'VMS') {
    print "ok 18 # Skip: SIGCONT not trappable in $^O\n";
} else {
    $newaction=POSIX::SigAction->new(sub { $ok10=1; });
    if (eval { SIGCONT; 1 }) {
	sigaction(SIGCONT, POSIX::SigAction->new('DEFAULT'));
	{
	    local($^W)=0;
	    kill 'CONT', $$;
	}
    }
    print $bad18 ? "not ok 18\n" : "ok 18\n";
}

{
    local $SIG{__WARN__} = sub { }; # Just suffer silently.

    my $hup20;
    my $hup21;

    sub hup20 { $hup20++ }
    sub hup21 { $hup21++ }

    sigaction("FOOBAR", $newaction);
    print "ok 19\n"; # no coredump, still alive

    $newaction = POSIX::SigAction->new("hup20");
    sigaction("SIGHUP", $newaction);
    kill "HUP", $$;
    print $hup20 == 1 ? "ok 20\n" : "not ok 20\n";

    $newaction = POSIX::SigAction->new("hup21");
    sigaction("HUP", $newaction);
    kill "HUP", $$;
    print $hup21 == 1 ? "ok 21\n" : "not ok 21\n";
}

# "safe" attribute.
# for this one, use the accessor instead of the attribute

# standard signal handling via %SIG is safe
$SIG{HUP} = \&foo;
$oldaction = POSIX::SigAction->new;
sigaction(SIGHUP, undef, $oldaction);
print $oldaction->safe ? "ok 22\n" : "not ok 22\n";

# SigAction handling is not safe ...
sigaction(SIGHUP, POSIX::SigAction->new(\&foo));
sigaction(SIGHUP, undef, $oldaction);
print $oldaction->safe ? "not ok 23\n" : "ok 23\n";

# ... unless we say so!
$newaction = POSIX::SigAction->new(\&foo);
$newaction->safe(1);
sigaction(SIGHUP, $newaction);
sigaction(SIGHUP, undef, $oldaction);
print $oldaction->safe ? "ok 24\n" : "not ok 24\n";

# And safe signal delivery must work
$ok = 0;
kill 'HUP', $$;
print $ok ? "ok 25\n" : "not ok 25\n";
