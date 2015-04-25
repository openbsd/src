#!./perl

# If a read or write is interrupted by a signal, Perl will call the
# signal handler and then attempt to restart the call. If the handler does
# something nasty like close the handle or pop layers, make sure that the
# read/write handles this gracefully (for some definition of 'graceful':
# principally, don't segfault).

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use warnings;
use strict;
use Config;

require './test.pl';

my $piped;
eval {
	pipe my $in, my $out;
	$piped = 1;
};
if (!$piped) {
	skip_all('pipe not implemented');
	exit 0;
}
unless (exists  $Config{'d_alarm'}) {
	skip_all('alarm not implemented');
	exit 0;
}

# XXX for some reason the stdio layer doesn't seem to interrupt
# write system call when the alarm triggers.  This makes the tests
# hang.

if (exists $ENV{PERLIO} && $ENV{PERLIO} =~ /stdio/  ) {
	skip_all('stdio not supported for this script');
	exit 0;
}

# on Win32, alarm() won't interrupt the read/write call.
# Similar issues with VMS.
# On FreeBSD, writes to pipes of 8192 bytes or more use a mechanism
# that is not interruptible (see perl #85842 and #84688).
# "close during print" also hangs on Solaris 8 (but not 10 or 11).
#
# Also skip on release builds, to avoid other possibly problematic
# platforms

my ($osmajmin) = $Config{osvers} =~ /^(\d+\.\d+)/;
if ($^O eq 'VMS' || $^O eq 'MSWin32' || $^O eq 'cygwin' || $^O =~ /freebsd/ || $^O eq 'midnightbsd' ||
     ($^O eq 'solaris' && $Config{osvers} eq '2.8') || $^O eq 'nto' ||
     ($^O eq 'darwin' && $osmajmin < 9) ||
    ((int($]*1000) & 1) == 0)
) {
	skip_all('various portability issues');
	exit 0;
}

my ($in, $out, $st, $sigst, $buf);

plan(tests => 10);


# make two handles that will always block

sub fresh_io {
	undef $in; undef $out; # use fresh handles each time
	pipe $in, $out;
	$sigst = "";
}

$SIG{PIPE} = 'IGNORE';

# close during read

fresh_io;
$SIG{ALRM} = sub { $sigst = close($in) ? "ok" : "nok" };
alarm(1);
$st = read($in, $buf, 1);
alarm(0);
is($sigst, 'ok', 'read/close: sig handler close status');
ok(!$st, 'read/close: read status');
ok(!close($in), 'read/close: close status');

# die during read

fresh_io;
$SIG{ALRM} = sub { die };
alarm(1);
$st = eval { read($in, $buf, 1) };
alarm(0);
ok(!$st, 'read/die: read status');
ok(close($in), 'read/die: close status');

# This used to be 1_000_000, but on Linux/ppc64 (POWER7) this kept
# consistently failing. At exactly 0x100000 it started passing
# again. We're hoping this number is bigger than any pipe buffer.
my $surely_this_arbitrary_number_is_fine = 0x100000;

# close during print

fresh_io;
$SIG{ALRM} = sub { $sigst = close($out) ? "ok" : "nok" };
$buf = "a" x $surely_this_arbitrary_number_is_fine . "\n";
select $out; $| = 1; select STDOUT;
alarm(1);
$st = print $out $buf;
alarm(0);
is($sigst, 'nok', 'print/close: sig handler close status');
ok(!$st, 'print/close: print status');
ok(!close($out), 'print/close: close status');

# die during print

fresh_io;
$SIG{ALRM} = sub { die };
$buf = "a" x $surely_this_arbitrary_number_is_fine . "\n";
select $out; $| = 1; select STDOUT;
alarm(1);
$st = eval { print $out $buf };
alarm(0);
ok(!$st, 'print/die: print status');
# the close will hang since there's data to flush, so use alarm
alarm(1);
ok(!eval {close($out)}, 'print/die: close status');
alarm(0);

# close during close

# Apparently there's nothing in standard Linux that can cause an
# EINTR in close(2); but run the code below just in case it does on some
# platform, just to see if it segfaults.
fresh_io;
$SIG{ALRM} = sub { $sigst = close($in) ? "ok" : "nok" };
alarm(1);
close $in;
alarm(0);

# die during close

fresh_io;
$SIG{ALRM} = sub { die };
alarm(1);
eval { close $in };
alarm(0);

# vim: ts=4 sts=4 sw=4:
