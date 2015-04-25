#!./perl

BEGIN {
    require Config; import Config;
    if ($^O ne 'VMS' and $Config{'extensions'} !~ /\bPOSIX\b/) {
	print "1..0\n";
	exit 0;
    }
}

use Test::More tests => 115;

use POSIX qw(fcntl_h signal_h limits_h _exit getcwd open read strftime write
	     errno localeconv dup dup2 lseek access);
use strict 'subs';

sub next_test {
    my $builder = Test::More->builder;
    $builder->current_test($builder->current_test() + 1);
}

$| = 1;

$Is_W32     = $^O eq 'MSWin32';
$Is_Dos     = $^O eq 'dos';
$Is_MacOS   = $^O eq 'MacOS';
$Is_VMS     = $^O eq 'VMS';
$Is_OS2     = $^O eq 'os2';
$Is_UWin    = $^O eq 'uwin';
$Is_OS390   = $^O eq 'os390';

my $vms_unix_rpt = 0;
my $vms_efs = 0;
my $unix_mode = 1;

if ($Is_VMS) {
    $unix_mode = 0;
    if (eval 'require VMS::Feature') {
        $vms_unix_rpt = VMS::Feature::current("filename_unix_report");
        $vms_efs = VMS::Feature::current("efs_charset");
    } else {
        my $unix_rpt = $ENV{'DECC$FILENAME_UNIX_REPORT'} || '';
        my $efs_charset = $ENV{'DECC$EFS_CHARSET'} || '';
        $vms_unix_rpt = $unix_rpt =~ /^[ET1]/i;
        $vms_efs = $efs_charset =~ /^[ET1]/i;
    }

    # Traditional VMS mode only if VMS is not in UNIX compatible mode.
    $unix_mode = ($vms_efs && $vms_unix_rpt);

}

my $testfd = open("Makefile.PL", O_RDONLY, 0);
like($testfd, qr/\A\d+\z/, 'O_RDONLY with open');
read($testfd, $buffer, 4) if $testfd > 2;
is( $buffer, "# Ex",                      '    with read' );

TODO:
{
    local $TODO = "read to array element not working";

    read($testfd, $buffer[1], 5) if $testfd > 2;
    is( $buffer[1], "perl\n",	               '    read to array element' );
}

my $test = next_test();
write(1,"ok $test\nnot ok $test\n", 5);

SKIP: {
    skip("no pipe() support on DOS", 2) if $Is_Dos;

    @fds = POSIX::pipe();
    cmp_ok($fds[0], '>', $testfd, 'POSIX::pipe');

    CORE::open($reader = \*READER, "<&=".$fds[0]);
    CORE::open($writer = \*WRITER, ">&=".$fds[1]);
    my $test = next_test();
    print $writer "ok $test\n";
    close $writer;
    print <$reader>;
    close $reader;
}

SKIP: {
    skip("no sigaction support on win32/dos", 6) if $Is_W32 || $Is_Dos;

    my $sigset = new POSIX::SigSet 1, 3;
    $sigset->delset(1);
    ok(! $sigset->ismember(1),  'POSIX::SigSet->delset' );
    ok(  $sigset->ismember(3),  'POSIX::SigSet->ismember' );

    SKIP: {
        skip("no kill() support on Mac OS", 4) if $Is_MacOS;

        my $sigint_called = 0;

	my $mask   = new POSIX::SigSet &SIGINT;
	my $action = new POSIX::SigAction 'main::SigHUP', $mask, 0;
	sigaction(&SIGHUP, $action);
	$SIG{'INT'} = 'SigINT';

	# At least OpenBSD/i386 3.3 is okay, as is NetBSD 1.5.
	# But not NetBSD 1.6 & 1.6.1: the test makes perl crash.
	# So the kill() must not be done with this config in order to
	# finish the test.
	# For others (darwin & freebsd), let the test fail without crashing.
	# the test passes at least from freebsd 8.1
	my $todo = $^O eq 'netbsd' && $Config{osvers}=~/^1\.6/;
	my $why_todo = "# TODO $^O $Config{osvers} seems to lose blocked signals";
	if (!$todo) { 
	  kill 'HUP', $$; 
	} else {
	  print "not ok 9 - sigaction SIGHUP ",$why_todo,"\n";
	  print "not ok 10 - sig mask delayed SIGINT ",$why_todo,"\n";
	}
	sleep 1;

	$todo = 1 if ($^O eq 'freebsd' && $Config{osvers} < 8)
		  || ($^O eq 'darwin' && $Config{osvers} < '6.6');
	printf "%s 11 - masked SIGINT received %s\n",
	    $sigint_called ? "ok" : "not ok",
	    $todo ? $why_todo : '';

	print "ok 12 - signal masks successful\n";
	
	sub SigHUP {
	    print "ok 9 - sigaction SIGHUP\n";
	    kill 'INT', $$;
	    sleep 2;
	    print "ok 10 - sig mask delayed SIGINT\n";
	}

        sub SigINT {
            $sigint_called++;
	}

        # The order of the above tests is very important, so
        # we use literal prints and hard coded numbers.
        next_test() for 1..4;
    }
}

SKIP: {
    skip("_POSIX_OPEN_MAX undefined ($fds[1])",  1) unless &_POSIX_OPEN_MAX;

    cmp_ok(&_POSIX_OPEN_MAX, '>=', 16,
	   "The minimum allowed values according to susv2" );

}

my $pat;
if ( $unix_mode ) {
    $pat = qr#[\\/]POSIX$#i;
}
else {
    $pat = qr/\.POSIX]/i;
}
like( getcwd(), qr/$pat/, 'getcwd' );

# Check string conversion functions.

SKIP: { 
    skip("strtod() not present", 2) unless $Config{d_strtod};

    if ($Config{d_setlocale}) {
        $lc = &POSIX::setlocale(&POSIX::LC_NUMERIC);
        &POSIX::setlocale(&POSIX::LC_NUMERIC, 'C');
    }

    # we're just checking that strtod works, not how accurate it is
    ($n, $x) = &POSIX::strtod('3.14159_OR_SO');
    cmp_ok(abs("3.14159" - $n), '<', 1e-6, 'strtod works');
    is($x, 6, 'strtod works');

    &POSIX::setlocale(&POSIX::LC_NUMERIC, $lc) if $Config{d_setlocale};
}

SKIP: {
    skip("strtol() not present", 2) unless $Config{d_strtol};

    ($n, $x) = &POSIX::strtol('21_PENGUINS');
    is($n, 21, 'strtol() number');
    is($x, 9,  '         unparsed chars');
}

SKIP: {
    skip("strtoul() not present", 2) unless $Config{d_strtoul};

    ($n, $x) = &POSIX::strtoul('88_TEARS');
    is($n, 88, 'strtoul() number');
    is($x, 6,  '          unparsed chars');
}

# Pick up whether we're really able to dynamically load everything.
cmp_ok(&POSIX::acos(1.0), '==', 0.0, 'dynamic loading');

# This can coredump if struct tm has a timezone field and we
# didn't detect it.  If this fails, try adding
# -DSTRUCT_TM_HASZONE to your cflags when compiling ext/POSIX/POSIX.c.
# See ext/POSIX/hints/sunos_4.pl and ext/POSIX/hints/linux.pl 
$test = next_test();
print POSIX::strftime("ok $test # %H:%M, on %m/%d/%y\n", localtime());

# If that worked, validate the mini_mktime() routine's normalisation of
# input fields to strftime().
sub try_strftime {
    my $expect = shift;
    my $got = POSIX::strftime("%a %b %d %H:%M:%S %Y %j", @_);
    is($got, $expect, "validating mini_mktime() and strftime(): $expect");
}

if ($Config{d_setlocale}) {
    $lc = &POSIX::setlocale(&POSIX::LC_TIME);
    &POSIX::setlocale(&POSIX::LC_TIME, 'C');
}

try_strftime("Wed Feb 28 00:00:00 1996 059", 0,0,0, 28,1,96);
SKIP: {
    skip("VC++ 8 and Vista's CRTs regard 60 seconds as an invalid parameter", 1)
	if ($Is_W32
	    and (($Config{cc} eq 'cl' and
		    $Config{ccversion} =~ /^(\d+)/ and $1 >= 14)
		or ($Config{cc} eq 'icl' and
		    `cl --version 2>&1` =~ /^.*Version\s+([\d.]+)/ and $1 >= 14)
		or (Win32::GetOSVersion())[1] >= 6));

    try_strftime("Thu Feb 29 00:00:60 1996 060", 60,0,-24, 30,1,96);
}
try_strftime("Fri Mar 01 00:00:00 1996 061", 0,0,-24, 31,1,96);
try_strftime("Sun Feb 28 00:00:00 1999 059", 0,0,0, 28,1,99);
try_strftime("Mon Mar 01 00:00:00 1999 060", 0,0,24, 28,1,99);
try_strftime("Mon Feb 28 00:00:00 2000 059", 0,0,0, 28,1,100);
try_strftime("Tue Feb 29 00:00:00 2000 060", 0,0,0, 0,2,100);
try_strftime("Wed Mar 01 00:00:00 2000 061", 0,0,0, 1,2,100);
try_strftime("Fri Mar 31 00:00:00 2000 091", 0,0,0, 31,2,100);

{ # rt 72232

  # Std C/POSIX allows day/month to be negative and requires that
  # wday/yday be adjusted as needed
  # previously mini_mktime() would allow yday to dominate if mday and
  # month were both non-positive
  # check that yday doesn't dominate
  try_strftime("Thu Dec 30 00:00:00 1999 364", 0,0,0, -1,0,100);
  try_strftime("Thu Dec 30 00:00:00 1999 364", 0,0,0, -1,0,100,-1,10);
  # it would also allow a positive wday to override the calculated value
  # check that wday is recalculated too
  try_strftime("Thu Dec 30 00:00:00 1999 364", 0,0,0, -1,0,100,0,10);
}

&POSIX::setlocale(&POSIX::LC_TIME, $lc) if $Config{d_setlocale};

{
    for my $test (0, 1) {
	$! = 0;
	# POSIX::errno is autoloaded. 
	# Autoloading requires many system calls.
	# errno() looks at $! to generate its result.
	# Autoloading should not munge the value.
	my $foo  = $!;
	my $errno = POSIX::errno();

        # Force numeric context.
	is( $errno + 0, $foo + 0,     'autoloading and errno() mix' );
    }
}

SKIP: {
  skip("no kill() support on Mac OS", 1) if $Is_MacOS;
  is (eval "kill 0", 0, "check we have CORE::kill")
    or print "\$\@ is " . _qq($@) . "\n";
}

# Check that we can import the POSIX kill routine
POSIX->import ('kill');
my $result = eval "kill 0";
is ($result, undef, "we should now have POSIX::kill");
# Check usage.
like ($@, qr/^Usage: POSIX::kill\(pid, sig\)/, "check its usage message");

# Check unimplemented.
$result = eval {POSIX::offsetof};
is ($result, undef, "offsetof should fail");
like ($@, qr/^Unimplemented: POSIX::offsetof\(\) is C-specific/,
      "check its unimplemented message");

# Check reimplemented.
$result = eval {POSIX::fgets};
is ($result, undef, "fgets should fail");
like ($@, qr/^Use method IO::Handle::gets\(\) instead/,
      "check its redef message");

{
    no warnings 'deprecated';
    # Simplistic tests for the isXXX() functions (bug #16799)
    ok( POSIX::isalnum('1'),  'isalnum' );
    ok(!POSIX::isalnum('*'),  'isalnum' );
    ok( POSIX::isalpha('f'),  'isalpha' );
    ok(!POSIX::isalpha('7'),  'isalpha' );
    ok( POSIX::iscntrl("\cA"),'iscntrl' );
    ok(!POSIX::iscntrl("A"),  'iscntrl' );
    ok( POSIX::isdigit('1'),  'isdigit' );
    ok(!POSIX::isdigit('z'),  'isdigit' );
    ok( POSIX::isgraph('@'),  'isgraph' );
    ok(!POSIX::isgraph(' '),  'isgraph' );
    ok( POSIX::islower('l'),  'islower' );
    ok(!POSIX::islower('L'),  'islower' );
    ok( POSIX::isupper('U'),  'isupper' );
    ok(!POSIX::isupper('u'),  'isupper' );
    ok( POSIX::isprint('$'),  'isprint' );
    ok(!POSIX::isprint("\n"), 'isprint' );
    ok( POSIX::ispunct('%'),  'ispunct' );
    ok(!POSIX::ispunct('u'),  'ispunct' );
    ok( POSIX::isspace("\t"), 'isspace' );
    ok(!POSIX::isspace('_'),  'isspace' );
    ok( POSIX::isxdigit('f'), 'isxdigit' );
    ok(!POSIX::isxdigit('g'), 'isxdigit' );
    # metaphysical question : what should be returned for an empty string ?
    # anyway this shouldn't segfault (bug #24554)
    ok( POSIX::isalnum(''),   'isalnum empty string' );
    ok( POSIX::isalnum(undef),'isalnum undef' );
    # those functions should stringify their arguments
    ok(!POSIX::isalpha([]),   'isalpha []' );
    ok( POSIX::isprint([]),   'isprint []' );
}

eval { use strict; POSIX->import("S_ISBLK"); my $x = S_ISBLK };
unlike( $@, qr/Can't use string .* as a symbol ref/, "Can import autoloaded constants" );

SKIP: {
    skip("localeconv() not present", 20) unless $Config{d_locconv};
    my $conv = localeconv;
    is(ref $conv, 'HASH', 'localconv returns a hash reference');

    foreach (qw(decimal_point thousands_sep grouping int_curr_symbol
		currency_symbol mon_decimal_point mon_thousands_sep
		mon_grouping positive_sign negative_sign)) {
    SKIP: {
	    skip("localeconv has no result for $_", 1)
		unless exists $conv->{$_};
	    unlike(delete $conv->{$_}, qr/\A\z/,
		   "localeconv returned a non-empty string for $_");
	}
    }

    my @lconv = qw(
        int_frac_digits frac_digits
        p_cs_precedes   p_sep_by_space
        n_cs_precedes   n_sep_by_space
        p_sign_posn     n_sign_posn
    );

    SKIP: {
        skip('No HAS_LC_MONETARY_2008', 6) unless $Config{d_lc_monetary_2008};

        push @lconv, qw(
            int_p_cs_precedes int_p_sep_by_space
            int_n_cs_precedes int_n_sep_by_space
            int_p_sign_posn   int_n_sign_posn
        );
    }
        
    foreach (@lconv) {
    SKIP: {
	    skip("localeconv has no result for $_", 1)
		unless exists $conv->{$_};
	    like(delete $conv->{$_}, qr/\A-?\d+\z/,
		 "localeconv returned an integer for $_");
	}
    }
    is_deeply([%$conv], [], 'no unexpected keys returned by localeconv');
}

my $fd1 = open("Makefile.PL", O_RDONLY, 0);
like($fd1, qr/\A\d+\z/, 'O_RDONLY with open');
cmp_ok($fd1, '>', $testfd);
my $fd2 = dup($fd1);
like($fd2, qr/\A\d+\z/, 'dup');
cmp_ok($fd2, '>', $fd1);
is(POSIX::close($fd1), '0 but true', 'close');
is(POSIX::close($testfd), '0 but true', 'close');
$! = 0;
undef $buffer;
is(read($fd1, $buffer, 4), undef, 'read on closed file handle fails');
cmp_ok($!, '==', POSIX::EBADF);
undef $buffer;
read($fd2, $buffer, 4) if $fd2 > 2;
is($buffer, "# Ex", 'read');
# The descriptor $testfd was using is now free, and is lower than that which
# $fd1 was using. Hence if dup2() behaves as dup(), we'll know :-)
{
    $testfd = dup2($fd2, $fd1);
    is($testfd, $fd1, 'dup2');
    undef $buffer;
    read($testfd, $buffer, 4) if $testfd > 2;
    is($buffer, 'pect', 'read');
    is(lseek($testfd, 0, 0), 0, 'lseek back');
    # The two should share file position:
    undef $buffer;
    read($fd2, $buffer, 4) if $fd2 > 2;
    is($buffer, "# Ex", 'read');
}

# The FreeBSD man page warns:
# The access() system call is a potential security hole due to race
# conditions and should never be used.
is(access('Makefile.PL', POSIX::F_OK), '0 but true', 'access');
is(access('Makefile.PL', POSIX::R_OK), '0 but true', 'access');
$! = 0;
is(access('no such file', POSIX::F_OK), undef, 'access on missing file');
cmp_ok($!, '==', POSIX::ENOENT);
is(access('Makefile.PL/nonsense', POSIX::F_OK), undef,
   'access on not-a-directory');
SKIP: {
    skip("$^O is insufficiently POSIX", 1)
	if $Is_W32 || $Is_VMS;
    cmp_ok($!, '==', POSIX::ENOTDIR);
}

# Check that output is not flushed by _exit. This test should be last
# in the file, and is not counted in the total number of tests.
if ($^O eq 'vos') {
 print "# TODO - hit VOS bug posix-885 - _exit flushes output buffers.\n";
} else {
 $| = 0;
 # The following line assumes buffered output, which may be not true:
 print '@#!*$@(!@#$' unless ($Is_MacOS || $Is_OS2 || $Is_UWin || $Is_OS390 ||
                            $Is_VMS ||
			    (defined $ENV{PERLIO} &&
			     $ENV{PERLIO} eq 'unix' &&
			     $Config::Config{useperlio}));
 _exit(0);
}
