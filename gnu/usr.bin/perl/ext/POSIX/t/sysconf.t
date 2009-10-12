#!perl -T

BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir 't';
        @INC = '../lib';
    }

    use Config;
    use Test::More;
    plan skip_all => "POSIX is unavailable" if $Config{'extensions'} !~ m!\bPOSIX\b!;
}

use strict;
use File::Spec;
use POSIX;
use Scalar::Util qw(looks_like_number);

sub check(@) {
    grep { eval "&$_;1" or $@!~/vendor has not defined POSIX macro/ } @_
}       

my @path_consts = check qw(
    _PC_CHOWN_RESTRICTED _PC_LINK_MAX _PC_NAME_MAX
    _PC_NO_TRUNC _PC_PATH_MAX
);

my @path_consts_terminal = check qw(
    _PC_MAX_CANON _PC_MAX_INPUT _PC_VDISABLE
);

my @path_consts_fifo = check qw(
    _PC_PIPE_BUF
);

my @sys_consts = check qw(
    _SC_ARG_MAX _SC_CHILD_MAX _SC_CLK_TCK _SC_JOB_CONTROL
    _SC_NGROUPS_MAX _SC_OPEN_MAX _SC_PAGESIZE _SC_SAVED_IDS
    _SC_STREAM_MAX _SC_VERSION _SC_TZNAME_MAX
);

my $tests = 2 * 3 * @path_consts +
            2 * 3 * @path_consts_terminal +
            2 * 3 * @path_consts_fifo +
                3 * @sys_consts;
plan $tests 
     ? (tests => $tests) 
     : (skip_all => "No tests to run on this OS")
;

# Don't test on "." as it can be networked storage which returns EINVAL
# Testing on "/" may not be portable to non-Unix as it may not be readable
# "/tmp" should be readable and likely also local.
my $testdir = File::Spec->tmpdir;
$testdir = VMS::Filespec::fileify($testdir) if $^O eq 'VMS';

my $r;

my $TTY = "/dev/tty";

sub _check_and_report {
    my ($eval_status, $return_val, $description) = @_;
    my $success = defined($return_val) || $! == 0;
    is( $eval_status, '', $description );
    SKIP: {
	skip "terminal constants set errno on QNX", 1
	    if $^O eq 'nto' and $description =~ $TTY;
        ok( $success, "\tchecking that the returned value is defined (" 
                        . (defined($return_val) ? "yes, it's $return_val)" : "it isn't)"
                        . " or that errno is clear ("
                        . (!($!+0) ? "it is)" : "it isn't, it's $!)"))
                        );
    }
    SKIP: {
        skip "constant not implemented on $^O or no limit in effect", 1 
            if !defined($return_val);
        ok( looks_like_number($return_val), "\tchecking that the returned value looks like a number" );
    }
}

# testing fpathconf() on a non-terminal file
SKIP: {
    my $fd = POSIX::open($testdir, O_RDONLY)
        or skip "could not open test directory '$testdir' ($!)",
	  3 * @path_consts;

    for my $constant (@path_consts) {
	    $! = 0;
            $r = eval { fpathconf( $fd, eval "$constant()" ) };
            _check_and_report( $@, $r, "calling fpathconf($fd, $constant) " );
    }
    
    POSIX::close($fd);
}

# testing pathconf() on a non-terminal file
for my $constant (@path_consts) {
	$! = 0;
        $r = eval { pathconf( $testdir, eval "$constant()" ) };
        _check_and_report( $@, $r, qq[calling pathconf("$testdir", $constant)] );
}

SKIP: {
    my $n = 2 * 3 * @path_consts_terminal;

    -c $TTY
	or skip("$TTY not a character file", $n);
    open(TTY, $TTY)
	or skip("failed to open $TTY: $!", $n);
    -t TTY
	or skip("TTY ($TTY) not a terminal file", $n);

    my $fd = fileno(TTY);

    # testing fpathconf() on a terminal file
    for my $constant (@path_consts_terminal) {
	$! = 0;
	$r = eval { fpathconf( $fd, eval "$constant()" ) };
	_check_and_report( $@, $r, qq[calling fpathconf($fd, $constant) ($TTY)] );
    }
    
    close($fd);
    # testing pathconf() on a terminal file
    for my $constant (@path_consts_terminal) {
	$! = 0;
	$r = eval { pathconf( $TTY, eval "$constant()" ) };
	_check_and_report( $@, $r, qq[calling pathconf($TTY, $constant)] );
    }
}

my $fifo = "fifo$$";

SKIP: {
    eval { mkfifo($fifo, 0666) }
	or skip("could not create fifo $fifo ($!)", 2 * 3 * @path_consts_fifo);

  SKIP: {
      my $fd = POSIX::open($fifo, O_RDWR)
	  or skip("could not open $fifo ($!)", 3 * @path_consts_fifo);

      for my $constant (@path_consts_fifo) {
	  $! = 0;
	  $r = eval { fpathconf( $fd, eval "$constant()" ) };
	  _check_and_report( $@, $r, "calling fpathconf($fd, $constant) ($fifo)" );
      }
    
      POSIX::close($fd);
  }

  # testing pathconf() on a fifo file
  for my $constant (@path_consts_fifo) {
      $! = 0;
      $r = eval { pathconf( $fifo, eval "$constant()" ) };
      _check_and_report( $@, $r, qq[calling pathconf($fifo, $constant)] );
  }
}

END {
    1 while unlink($fifo);
}

SKIP: {
    if($^O eq 'cygwin') {
        pop @sys_consts;
        skip("No _SC_TZNAME_MAX on Cygwin", 3);
    }
        
}
# testing sysconf()
for my $constant (@sys_consts) {
	$! = 0;
	$r = eval { sysconf( eval "$constant()" ) };
	_check_and_report( $@, $r, "calling sysconf($constant)" );
}

