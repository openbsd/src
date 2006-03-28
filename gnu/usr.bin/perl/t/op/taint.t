#!./perl -T
#
# Taint tests by Tom Phoenix <rootbeer@teleport.com>.
#
# I don't claim to know all about tainting. If anyone sees
# tests that I've missed here, please add them. But this is
# better than having no tests at all, right?
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use strict;
use Config;
use File::Spec::Functions;

BEGIN { require './test.pl'; }
plan tests => 238;


$| = 1;

use vars qw($ipcsysv); # did we manage to load IPC::SysV?

BEGIN {
  if ($^O eq 'VMS' && !defined($Config{d_setenv})) {
      $ENV{PATH} = $ENV{PATH};
      $ENV{TERM} = $ENV{TERM} ne ''? $ENV{TERM} : 'dummy';
  }
  if ($Config{'extensions'} =~ /\bIPC\/SysV\b/
      && ($Config{d_shm} || $Config{d_msg})) {
      eval { require IPC::SysV };
      unless ($@) {
	  $ipcsysv++;
	  IPC::SysV->import(qw(IPC_PRIVATE IPC_RMID IPC_CREAT S_IRWXU IPC_NOWAIT));
      }
  }
}

my $Is_MacOS    = $^O eq 'MacOS';
my $Is_VMS      = $^O eq 'VMS';
my $Is_MSWin32  = $^O eq 'MSWin32';
my $Is_NetWare  = $^O eq 'NetWare';
my $Is_Dos      = $^O eq 'dos';
my $Is_Cygwin   = $^O eq 'cygwin';
my $Invoke_Perl = $Is_VMS      ? 'MCR Sys$Disk:[]Perl.' :
                  $Is_MSWin32  ? '.\perl'               :
                  $Is_MacOS    ? ':perl'                :
                  $Is_NetWare  ? 'perl'                 : 
                                 './perl'               ;
my @MoreEnv = qw/IFS CDPATH ENV BASH_ENV/;

if ($Is_VMS) {
    my (%old, $x);
    for $x ('DCL$PATH', @MoreEnv) {
	($old{$x}) = $ENV{$x} =~ /^(.*)$/ if exists $ENV{$x};
    }
    eval <<EndOfCleanup;
	END {
	    \$ENV{PATH} = '' if $Config{d_setenv};
	    warn "# Note: logical name 'PATH' may have been deleted\n";
	    \@ENV{keys %old} = values %old;
	}
EndOfCleanup
}

# Sources of taint:
#   The empty tainted value, for tainting strings
my $TAINT = substr($^X, 0, 0);
#   A tainted zero, useful for tainting numbers
my $TAINT0;
{
    no warnings;
    $TAINT0 = 0 + $TAINT;
}

# This taints each argument passed. All must be lvalues.
# Side effect: It also stringifies them. :-(
sub taint_these (@) {
    for (@_) { $_ .= $TAINT }
}

# How to identify taint when you see it
sub any_tainted (@) {
    not eval { join("",@_), kill 0; 1 };
}
sub tainted ($) {
    any_tainted @_;
}
sub all_tainted (@) {
    for (@_) { return 0 unless tainted $_ }
    1;
}


sub test ($;$) {
    my($ok, $diag) = @_;

    my $curr_test = curr_test();

    if ($ok) {
	print "ok $curr_test\n";
    } else {
	print "not ok $curr_test\n";
        printf "# Failed test at line %d\n", (caller)[2];
	for (split m/^/m, $diag) {
	    print "# $_";
	}
	print "\n" unless
	    $diag eq ''
	    or substr($diag, -1) eq "\n";
    }

    next_test();

    return $ok;
}

# We need an external program to call.
my $ECHO = ($Is_MSWin32 ? ".\\echo$$" : $Is_MacOS ? ":echo$$" : ($Is_NetWare ? "echo$$" : "./echo$$"));
END { unlink $ECHO }
open PROG, "> $ECHO" or die "Can't create $ECHO: $!";
print PROG 'print "@ARGV\n"', "\n";
close PROG;
my $echo = "$Invoke_Perl $ECHO";

my $TEST = catfile(curdir(), 'TEST');

# First, let's make sure that Perl is checking the dangerous
# environment variables. Maybe they aren't set yet, so we'll
# taint them ourselves.
{
    $ENV{'DCL$PATH'} = '' if $Is_VMS;

    if ($Is_MSWin32 && $Config{ccname} =~ /bcc32/ && ! -f 'cc3250mt.dll') {
	my $bcc_dir;
	foreach my $dir (split /$Config{path_sep}/, $ENV{PATH}) {
	    if (-f "$dir/cc3250mt.dll") {
		$bcc_dir = $dir and last;
	    }
	}
	if (defined $bcc_dir) {
	    require File::Copy;
	    File::Copy::copy("$bcc_dir/cc3250mt.dll", '.') or
		die "$0: failed to copy cc3250mt.dll: $!\n";
	    eval q{
		END { unlink "cc3250mt.dll" }
	    };
	}
    }

    $ENV{PATH} = '';
    delete @ENV{@MoreEnv};
    $ENV{TERM} = 'dumb';

    if ($Is_Cygwin && ! -f 'cygwin1.dll') {
	system("/usr/bin/cp /usr/bin/cygwin1.dll .") &&
	    die "$0: failed to cp cygwin1.dll: $!\n";
	eval q{
	    END { unlink "cygwin1.dll" }
	};
    }

    if ($Is_Cygwin && ! -f 'cygcrypt-0.dll' && -f '/usr/bin/cygcrypt-0.dll') {
	system("/usr/bin/cp /usr/bin/cygcrypt-0.dll .") &&
	    die "$0: failed to cp cygcrypt-0.dll: $!\n";
	eval q{
	    END { unlink "cygcrypt-0.dll" }
	};
    }

    test eval { `$echo 1` } eq "1\n";

    SKIP: {
        skip "Environment tainting tests skipped", 4
          if $Is_MSWin32 || $Is_NetWare || $Is_VMS || $Is_Dos || $Is_MacOS;

	my @vars = ('PATH', @MoreEnv);
	while (my $v = $vars[0]) {
	    local $ENV{$v} = $TAINT;
	    last if eval { `$echo 1` };
	    last unless $@ =~ /^Insecure \$ENV{$v}/;
	    shift @vars;
	}
	test !@vars, "@vars";

	# tainted $TERM is unsafe only if it contains metachars
	local $ENV{TERM};
	$ENV{TERM} = 'e=mc2';
	test eval { `$echo 1` } eq "1\n";
	$ENV{TERM} = 'e=mc2' . $TAINT;
	test !eval { `$echo 1` };
	test $@ =~ /^Insecure \$ENV{TERM}/, $@;
    }

    my $tmp;
    if ($^O eq 'os2' || $^O eq 'amigaos' || $Is_MSWin32 || $Is_NetWare || $Is_Dos) {
	print "# all directories are writeable\n";
    }
    else {
	$tmp = (grep { defined and -d and (stat _)[2] & 2 }
		     qw(sys$scratch /tmp /var/tmp /usr/tmp),
		     @ENV{qw(TMP TEMP)})[0]
	    or print "# can't find world-writeable directory to test PATH\n";
    }

    SKIP: {
        skip "all directories are writeable", 2 unless $tmp;

	local $ENV{PATH} = $tmp;
	test !eval { `$echo 1` };
	test $@ =~ /^Insecure directory in \$ENV{PATH}/, $@;
    }

    SKIP: {
        skip "This is not VMS", 4 unless $Is_VMS;

	$ENV{'DCL$PATH'} = $TAINT;
	test  eval { `$echo 1` } eq '';
	test $@ =~ /^Insecure \$ENV{DCL\$PATH}/, $@;
	SKIP: {
            skip q[can't find world-writeable directory to test DCL$PATH], 2
              unless $tmp;

	    $ENV{'DCL$PATH'} = $tmp;
	    test eval { `$echo 1` } eq '';
	    test $@ =~ /^Insecure directory in \$ENV{DCL\$PATH}/, $@;
	}
	$ENV{'DCL$PATH'} = '';
    }
}

# Let's see that we can taint and untaint as needed.
{
    my $foo = $TAINT;
    test tainted $foo;

    # That was a sanity check. If it failed, stop the insanity!
    die "Taint checks don't seem to be enabled" unless tainted $foo;

    $foo = "foo";
    test not tainted $foo;

    taint_these($foo);
    test tainted $foo;

    my @list = 1..10;
    test not any_tainted @list;
    taint_these @list[1,3,5,7,9];
    test any_tainted @list;
    test all_tainted @list[1,3,5,7,9];
    test not any_tainted @list[0,2,4,6,8];

    ($foo) = $foo =~ /(.+)/;
    test not tainted $foo;

    $foo = $1 if ('bar' . $TAINT) =~ /(.+)/;
    test not tainted $foo;
    test $foo eq 'bar';

    {
      use re 'taint';

      ($foo) = ('bar' . $TAINT) =~ /(.+)/;
      test tainted $foo;
      test $foo eq 'bar';

      $foo = $1 if ('bar' . $TAINT) =~ /(.+)/;
      test tainted $foo;
      test $foo eq 'bar';
    }

    $foo = $1 if 'bar' =~ /(.+)$TAINT/;
    test tainted $foo;
    test $foo eq 'bar';

    my $pi = 4 * atan2(1,1) + $TAINT0;
    test tainted $pi;

    ($pi) = $pi =~ /(\d+\.\d+)/;
    test not tainted $pi;
    test sprintf("%.5f", $pi) eq '3.14159';
}

# How about command-line arguments? The problem is that we don't
# always get some, so we'll run another process with some.
SKIP: {
    my $arg = catfile(curdir(), "arg$$");
    open PROG, "> $arg" or die "Can't create $arg: $!";
    print PROG q{
	eval { join('', @ARGV), kill 0 };
	exit 0 if $@ =~ /^Insecure dependency/;
	print "# Oops: \$@ was [$@]\n";
	exit 1;
    };
    close PROG;
    print `$Invoke_Perl "-T" $arg and some suspect arguments`;
    test !$?, "Exited with status $?";
    unlink $arg;
}

# Reading from a file should be tainted
{
    test open(FILE, $TEST), "Couldn't open '$TEST': $!";

    my $block;
    sysread(FILE, $block, 100);
    my $line = <FILE>;
    close FILE;
    test tainted $block;
    test tainted $line;
}

# Globs should be forbidden, except under VMS,
#   which doesn't spawn an external program.
SKIP: {
    skip "globs should be forbidden", 2 if 1 or $Is_VMS;

    my @globs = eval { <*> };
    test @globs == 0 && $@ =~ /^Insecure dependency/;

    @globs = eval { glob '*' };
    test @globs == 0 && $@ =~ /^Insecure dependency/;
}

# Output of commands should be tainted
{
    my $foo = `$echo abc`;
    test tainted $foo;
}

# Certain system variables should be tainted
{
    test all_tainted $^X, $0;
}

# Results of matching should all be untainted
{
    my $foo = "abcdefghi" . $TAINT;
    test tainted $foo;

    $foo =~ /def/;
    test not any_tainted $`, $&, $';

    $foo =~ /(...)(...)(...)/;
    test not any_tainted $1, $2, $3, $+;

    my @bar = $foo =~ /(...)(...)(...)/;
    test not any_tainted @bar;

    test tainted $foo;	# $foo should still be tainted!
    test $foo eq "abcdefghi";
}

# Operations which affect files can't use tainted data.
{
    test !eval { chmod 0, $TAINT }, 'chmod';
    test $@ =~ /^Insecure dependency/, $@;

    # There is no feature test in $Config{} for truncate,
    #   so we allow for the possibility that it's missing.
    test !eval { truncate 'NoSuChFiLe', $TAINT0 }, 'truncate';
    test $@ =~ /^(?:Insecure dependency|truncate not implemented)/, $@;

    test !eval { rename '', $TAINT }, 'rename';
    test $@ =~ /^Insecure dependency/, $@;

    test !eval { unlink $TAINT }, 'unlink';
    test $@ =~ /^Insecure dependency/, $@;

    test !eval { utime $TAINT }, 'utime';
    test $@ =~ /^Insecure dependency/, $@;

    SKIP: {
        skip "chown() is not available", 2 unless $Config{d_chown};

	test !eval { chown -1, -1, $TAINT }, 'chown';
	test $@ =~ /^Insecure dependency/, $@;
    }

    SKIP: {
        skip "link() is not available", 2 unless $Config{d_link};

	test !eval { link $TAINT, '' }, 'link';
	test $@ =~ /^Insecure dependency/, $@;
    }

    SKIP: {
        skip "symlink() is not available", 2 unless $Config{d_symlink};

	test !eval { symlink $TAINT, '' }, 'symlink';
	test $@ =~ /^Insecure dependency/, $@;
    }
}

# Operations which affect directories can't use tainted data.
{
    test !eval { mkdir "foo".$TAINT, 0755.$TAINT0 }, 'mkdir';
    test $@ =~ /^Insecure dependency/, $@;

    test !eval { rmdir $TAINT }, 'rmdir';
    test $@ =~ /^Insecure dependency/, $@;

    test !eval { chdir "foo".$TAINT }, 'chdir';
    test $@ =~ /^Insecure dependency/, $@;

    SKIP: {
        skip "chroot() is not available", 2 unless $Config{d_chroot};

	test !eval { chroot $TAINT }, 'chroot';
	test $@ =~ /^Insecure dependency/, $@;
    }
}

# Some operations using files can't use tainted data.
{
    my $foo = "imaginary library" . $TAINT;
    test !eval { require $foo }, 'require';
    test $@ =~ /^Insecure dependency/, $@;

    my $filename = "./taintB$$";	# NB: $filename isn't tainted!
    END { unlink $filename if defined $filename }
    $foo = $filename . $TAINT;
    unlink $filename;	# in any case

    test !eval { open FOO, $foo }, 'open for read';
    test $@ eq '', $@;		# NB: This should be allowed

    # Try first new style but allow also old style.
    # We do not want the whole taint.t to fail
    # just because Errno possibly failing.
    test eval('$!{ENOENT}') ||
	$! == 2 || # File not found
	($Is_Dos && $! == 22) ||
	($^O eq 'mint' && $! == 33);

    test !eval { open FOO, "> $foo" }, 'open for write';
    test $@ =~ /^Insecure dependency/, $@;
}

# Commands to the system can't use tainted data
{
    my $foo = $TAINT;

    SKIP: {
        skip "open('|') is not available", 4 if $^O eq 'amigaos';

	test !eval { open FOO, "| x$foo" }, 'popen to';
	test $@ =~ /^Insecure dependency/, $@;

	test !eval { open FOO, "x$foo |" }, 'popen from';
	test $@ =~ /^Insecure dependency/, $@;
    }

    test !eval { exec $TAINT }, 'exec';
    test $@ =~ /^Insecure dependency/, $@;

    test !eval { system $TAINT }, 'system';
    test $@ =~ /^Insecure dependency/, $@;

    $foo = "*";
    taint_these $foo;

    test !eval { `$echo 1$foo` }, 'backticks';
    test $@ =~ /^Insecure dependency/, $@;

    SKIP: {
        # wildcard expansion doesn't invoke shell on VMS, so is safe
        skip "This is not VMS", 2 unless $Is_VMS;
    
	test join('', eval { glob $foo } ) ne '', 'globbing';
	test $@ eq '', $@;
    }
}

# Operations which affect processes can't use tainted data.
{
    test !eval { kill 0, $TAINT }, 'kill';
    test $@ =~ /^Insecure dependency/, $@;

    SKIP: {
        skip "setpgrp() is not available", 2 unless $Config{d_setpgrp};

	test !eval { setpgrp 0, $TAINT0 }, 'setpgrp';
	test $@ =~ /^Insecure dependency/, $@;
    }

    SKIP: {
        skip "setpriority() is not available", 2 unless $Config{d_setprior};

	test !eval { setpriority 0, $TAINT0, $TAINT0 }, 'setpriority';
	test $@ =~ /^Insecure dependency/, $@;
    }
}

# Some miscellaneous operations can't use tainted data.
{
    SKIP: {
        skip "syscall() is not available", 2 unless $Config{d_syscall};

	test !eval { syscall $TAINT }, 'syscall';
	test $@ =~ /^Insecure dependency/, $@;
    }

    {
	my $foo = "x" x 979;
	taint_these $foo;
	local *FOO;
	my $temp = "./taintC$$";
	END { unlink $temp }
	test open(FOO, "> $temp"), "Couldn't open $temp for write: $!";

	test !eval { ioctl FOO, $TAINT0, $foo }, 'ioctl';
	test $@ =~ /^Insecure dependency/, $@;

        SKIP: {
            skip "fcntl() is not available", 2 unless $Config{d_fcntl};

	    test !eval { fcntl FOO, $TAINT0, $foo }, 'fcntl';
	    test $@ =~ /^Insecure dependency/, $@;
	}

	close FOO;
    }
}

# Some tests involving references
{
    my $foo = 'abc' . $TAINT;
    my $fooref = \$foo;
    test not tainted $fooref;
    test tainted $$fooref;
    test tainted $foo;
}

# Some tests involving assignment
{
    my $foo = $TAINT0;
    my $bar = $foo;
    test all_tainted $foo, $bar;
    test tainted($foo = $bar);
    test tainted($bar = $bar);
    test tainted($bar += $bar);
    test tainted($bar -= $bar);
    test tainted($bar *= $bar);
    test tainted($bar++);
    test tainted($bar /= $bar);
    test tainted($bar += 0);
    test tainted($bar -= 2);
    test tainted($bar *= -1);
    test tainted($bar /= 1);
    test tainted($bar--);
    test $bar == 0;
}

# Test assignment and return of lists
{
    my @foo = ("A", "tainted" . $TAINT, "B");
    test not tainted $foo[0];
    test     tainted $foo[1];
    test not tainted $foo[2];
    my @bar = @foo;
    test not tainted $bar[0];
    test     tainted $bar[1];
    test not tainted $bar[2];
    my @baz = eval { "A", "tainted" . $TAINT, "B" };
    test not tainted $baz[0];
    test     tainted $baz[1];
    test not tainted $baz[2];
    my @plugh = eval q[ "A", "tainted" . $TAINT, "B" ];
    test not tainted $plugh[0];
    test     tainted $plugh[1];
    test not tainted $plugh[2];
    my $nautilus = sub { "A", "tainted" . $TAINT, "B" };
    test not tainted ((&$nautilus)[0]);
    test     tainted ((&$nautilus)[1]);
    test not tainted ((&$nautilus)[2]);
    my @xyzzy = &$nautilus;
    test not tainted $xyzzy[0];
    test     tainted $xyzzy[1];
    test not tainted $xyzzy[2];
    my $red_october = sub { return "A", "tainted" . $TAINT, "B" };
    test not tainted ((&$red_october)[0]);
    test     tainted ((&$red_october)[1]);
    test not tainted ((&$red_october)[2]);
    my @corge = &$red_october;
    test not tainted $corge[0];
    test     tainted $corge[1];
    test not tainted $corge[2];
}

# Test for system/library calls returning string data of dubious origin.
{
    # No reliable %Config check for getpw*
    SKIP: {
        skip "getpwent() is not available", 1 unless 
          eval { setpwent(); getpwent() };

	setpwent();
	my @getpwent = getpwent();
	die "getpwent: $!\n" unless (@getpwent);
	test (    not tainted $getpwent[0]
	          and     tainted $getpwent[1]
	          and not tainted $getpwent[2]
	          and not tainted $getpwent[3]
	          and not tainted $getpwent[4]
	          and not tainted $getpwent[5]
	          and     tainted $getpwent[6]		# ge?cos
	          and not tainted $getpwent[7]
		  and     tainted $getpwent[8]);	# shell
	endpwent();
    }

    SKIP: {
        # pretty hard to imagine not
        skip "readdir() is not available", 1 unless $Config{d_readdir};

	local(*D);
	opendir(D, "op") or die "opendir: $!\n";
	my $readdir = readdir(D);
	test tainted $readdir;
	closedir(D);
    }

    SKIP: {
        skip "readlink() or symlink() is not available" unless 
          $Config{d_readlink} && $Config{d_symlink};

	my $symlink = "sl$$";
	unlink($symlink);
	my $sl = "/something/naughty";
	# it has to be a real path on Mac OS
	$sl = MacPerl::MakePath((MacPerl::Volumes())[0]) if $Is_MacOS;
	symlink($sl, $symlink) or die "symlink: $!\n";
	my $readlink = readlink($symlink);
	test tainted $readlink;
	unlink($symlink);
    }
}

# test bitwise ops (regression bug)
{
    my $why = "y";
    my $j = "x" | $why;
    test not tainted $j;
    $why = $TAINT."y";
    $j = "x" | $why;
    test     tainted $j;
}

# test target of substitution (regression bug)
{
    my $why = $TAINT."y";
    $why =~ s/y/z/;
    test     tainted $why;

    my $z = "[z]";
    $why =~ s/$z/zee/;
    test     tainted $why;

    $why =~ s/e/'-'.$$/ge;
    test     tainted $why;
}


SKIP: {
    skip "no IPC::SysV", 2 unless $ipcsysv;

    # test shmread
    SKIP: {
        skip "shm*() not available", 1 unless $Config{d_shm};

        no strict 'subs';
        my $sent = "foobar";
        my $rcvd;
        my $size = 2000;
        my $id = shmget(IPC_PRIVATE, $size, S_IRWXU);

        if (defined $id) {
            if (shmwrite($id, $sent, 0, 60)) {
                if (shmread($id, $rcvd, 0, 60)) {
                    substr($rcvd, index($rcvd, "\0")) = '';
                } else {
                    warn "# shmread failed: $!\n";
                }
            } else {
                warn "# shmwrite failed: $!\n";
            }
            shmctl($id, IPC_RMID, 0) or warn "# shmctl failed: $!\n";
        } else {
            warn "# shmget failed: $!\n";
        }

        skip "SysV shared memory operation failed", 1 unless 
          $rcvd eq $sent;

        test tainted $rcvd;
    }


    # test msgrcv
    SKIP: {
        skip "msg*() not available", 1 unless $Config{d_msg};

	no strict 'subs';
	my $id = msgget(IPC_PRIVATE, IPC_CREAT | S_IRWXU);

	my $sent      = "message";
	my $type_sent = 1234;
	my $rcvd;
	my $type_rcvd;

	if (defined $id) {
	    if (msgsnd($id, pack("l! a*", $type_sent, $sent), IPC_NOWAIT)) {
		if (msgrcv($id, $rcvd, 60, 0, IPC_NOWAIT)) {
		    ($type_rcvd, $rcvd) = unpack("l! a*", $rcvd);
		} else {
		    warn "# msgrcv failed: $!\n";
		}
	    } else {
		warn "# msgsnd failed: $!\n";
	    }
	    msgctl($id, IPC_RMID, 0) or warn "# msgctl failed: $!\n";
	} else {
	    warn "# msgget failed\n";
	}

        SKIP: {
            skip "SysV message queue operation failed", 1
              unless $rcvd eq $sent && $type_sent == $type_rcvd;

	    test tainted $rcvd;
	}
    }
}

{
    # bug id 20001004.006

    open IN, $TEST or warn "$0: cannot read $TEST: $!" ;
    local $/;
    my $a = <IN>;
    my $b = <IN>;

    ok tainted($a) && tainted($b) && !defined($b);

    close IN;
}

{
    # bug id 20001004.007

    open IN, $TEST or warn "$0: cannot read $TEST: $!" ;
    my $a = <IN>;

    my $c = { a => 42,
	      b => $a };

    ok !tainted($c->{a}) && tainted($c->{b});


    my $d = { a => $a,
	      b => 42 };
    ok tainted($d->{a}) && !tainted($d->{b});


    my $e = { a => 42,
	      b => { c => $a, d => 42 } };
    ok !tainted($e->{a}) &&
       !tainted($e->{b}) &&
	tainted($e->{b}->{c}) &&
       !tainted($e->{b}->{d});

    close IN;
}

{
    # bug id 20010519.003

    BEGIN {
	use vars qw($has_fcntl);
	eval { require Fcntl; import Fcntl; };
	unless ($@) {
	    $has_fcntl = 1;
	}
    }

    SKIP: {
        skip "no Fcntl", 18 unless $has_fcntl;

	my $evil = "foo" . $TAINT;

	eval { sysopen(my $ro, $evil, &O_RDONLY) };
	test $@ !~ /^Insecure dependency/, $@;
	
	eval { sysopen(my $wo, $evil, &O_WRONLY) };
	test $@ =~ /^Insecure dependency/, $@;
	
	eval { sysopen(my $rw, $evil, &O_RDWR) };
	test $@ =~ /^Insecure dependency/, $@;
	
	eval { sysopen(my $ap, $evil, &O_APPEND) };
	test $@ =~ /^Insecure dependency/, $@;
	
	eval { sysopen(my $cr, $evil, &O_CREAT) };
	test $@ =~ /^Insecure dependency/, $@;
	
	eval { sysopen(my $tr, $evil, &O_TRUNC) };
	test $@ =~ /^Insecure dependency/, $@;
	
	eval { sysopen(my $ro, "foo", &O_RDONLY | $TAINT0) };
	test $@ !~ /^Insecure dependency/, $@;
	
	eval { sysopen(my $wo, "foo", &O_WRONLY | $TAINT0) };
	test $@ =~ /^Insecure dependency/, $@;

	eval { sysopen(my $rw, "foo", &O_RDWR | $TAINT0) };
	test $@ =~ /^Insecure dependency/, $@;

	eval { sysopen(my $ap, "foo", &O_APPEND | $TAINT0) };
	test $@ =~ /^Insecure dependency/, $@;
	
	eval { sysopen(my $cr, "foo", &O_CREAT | $TAINT0) };
	test $@ =~ /^Insecure dependency/, $@;

	eval { sysopen(my $tr, "foo", &O_TRUNC | $TAINT0) };
	test $@ =~ /^Insecure dependency/, $@;

	eval { sysopen(my $ro, "foo", &O_RDONLY, $TAINT0) };
	test $@ !~ /^Insecure dependency/, $@;
	
	eval { sysopen(my $wo, "foo", &O_WRONLY, $TAINT0) };
	test $@ =~ /^Insecure dependency/, $@;
	
	eval { sysopen(my $rw, "foo", &O_RDWR, $TAINT0) };
	test $@ =~ /^Insecure dependency/, $@;
	
	eval { sysopen(my $ap, "foo", &O_APPEND, $TAINT0) };
	test $@ =~ /^Insecure dependency/, $@;
	
	eval { sysopen(my $cr, "foo", &O_CREAT, $TAINT0) };
	test $@ =~ /^Insecure dependency/, $@;

	eval { sysopen(my $tr, "foo", &O_TRUNC, $TAINT0) };
	test $@ =~ /^Insecure dependency/, $@;
	
	unlink("foo"); # not unlink($evil), because that would fail...
    }
}

{
    # bug 20010526.004

    use warnings;

    my $saw_warning = 0;
    local $SIG{__WARN__} = sub { $saw_warning = 1 };

    sub fmi {
	my $divnum = shift()/1;
	sprintf("%1.1f\n", $divnum);
    }

    fmi(21 . $TAINT);
    fmi(37);
    fmi(248);

    test !$saw_warning;
}


{
    # Bug ID 20010730.010

    my $i = 0;

    sub Tie::TIESCALAR {
        my $class =  shift;
        my $arg   =  shift;

        bless \$arg => $class;
    }

    sub Tie::FETCH {
        $i ++;
        ${$_ [0]}
    }

 
    package main;
 
    my $bar = "The Big Bright Green Pleasure Machine";
    taint_these $bar;
    tie my ($foo), Tie => $bar;

    my $baz = $foo;

    ok $i == 1;
}

{
    # Check that all environment variables are tainted.
    my @untainted;
    while (my ($k, $v) = each %ENV) {
	if (!tainted($v) &&
	    # These we have explicitly untainted or set earlier.
	    $k !~ /^(BASH_ENV|CDPATH|ENV|IFS|PATH|PERL_CORE|TEMP|TERM|TMP)$/) {
	    push @untainted, "# '$k' = '$v'\n";
	}
    }
    test @untainted == 0, "untainted:\n @untainted";
}


ok( ${^TAINT} == 1, '$^TAINT is on' );

eval { ${^TAINT} = 0 };
ok( ${^TAINT},  '$^TAINT is not assignable' );
ok( $@ =~ /^Modification of a read-only value attempted/,
                                'Assigning to ${^TAINT} fails' );

{
    # bug 20011111.105
    
    my $re1 = qr/x$TAINT/;
    test tainted $re1;
    
    my $re2 = qr/^$re1\z/;
    test tainted $re2;
    
    my $re3 = "$re2";
    test tainted $re3;
}

SKIP: {
    skip "system {} has different semantics on Win32", 1 if $Is_MSWin32;

    # bug 20010221.005
    local $ENV{PATH} .= $TAINT;
    eval { system { "echo" } "/arg0", "arg1" };
    test $@ =~ /^Insecure \$ENV/;
}

TODO: {
    todo_skip 'tainted %ENV warning occludes tainted arguments warning', 22
      if $Is_VMS;

    # bug 20020208.005 plus some single arg exec/system extras
    my $err = qr/^Insecure dependency/ ;
    test !eval { exec $TAINT, $TAINT }, 'exec';
    test $@ =~ $err, $@;
    test !eval { exec $TAINT $TAINT }, 'exec';
    test $@ =~ $err, $@;
    test !eval { exec $TAINT $TAINT, $TAINT }, 'exec';
    test $@ =~ $err, $@;
    test !eval { exec $TAINT 'notaint' }, 'exec';
    test $@ =~ $err, $@;
    test !eval { exec {'notaint'} $TAINT }, 'exec';
    test $@ =~ $err, $@;

    test !eval { system $TAINT, $TAINT }, 'system';
    test $@ =~ $err, $@;
    test !eval { system $TAINT $TAINT }, 'system';
    test $@ =~ $err, $@;
    test !eval { system $TAINT $TAINT, $TAINT }, 'system';
    test $@ =~ $err, $@;
    test !eval { system $TAINT 'notaint' }, 'system';
    test $@ =~ $err, $@;
    test !eval { system {'notaint'} $TAINT }, 'system';
    test $@ =~ $err, $@;

    eval { 
        no warnings;
        system("lskdfj does not exist","with","args"); 
    };
    test !$@;

    SKIP: {
        skip "no exec() on MacOS Classic" if $Is_MacOS;

	eval { 
            no warnings;
            exec("lskdfj does not exist","with","args"); 
        };
	test !$@;
    }

    # If you add tests here update also the above skip block for VMS.
}

{
    # [ID 20020704.001] taint propagation failure
    use re 'taint';
    $TAINT =~ /(.*)/;
    test tainted(my $foo = $1);
}

{
    # [perl #24291] this used to dump core
    our %nonmagicalenv = ( PATH => "util" );
    local *ENV = \%nonmagicalenv;
    eval { system("lskdfj"); };
    test $@ =~ /^%ENV is aliased to another variable while running with -T switch/;
    local *ENV = *nonmagicalenv;
    eval { system("lskdfj"); };
    test $@ =~ /^%ENV is aliased to %nonmagicalenv while running with -T switch/;
}
{
    # [perl #24248]
    $TAINT =~ /(.*)/;
    test !tainted($1);
    my $notaint = $1;
    test !tainted($notaint);

    my $l;
    $notaint =~ /($notaint)/;
    $l = $1;
    test !tainted($1);
    test !tainted($l);
    $notaint =~ /($TAINT)/;
    $l = $1;
    test tainted($1);
    test tainted($l);

    $TAINT =~ /($notaint)/;
    $l = $1;
    test !tainted($1);
    test !tainted($l);
    $TAINT =~ /($TAINT)/;
    $l = $1;
    test tainted($1);
    test tainted($l);

    my $r;
    ($r = $TAINT) =~ /($notaint)/;
    test !tainted($1);
    ($r = $TAINT) =~ /($TAINT)/;
    test tainted($1);

    #  [perl #24674]
    # accessing $^O  shoudn't taint it as a side-effect;
    # assigning tainted data to it is now an error

    test !tainted($^O);
    if (!$^X) { } elsif ($^O eq 'bar') { }
    test !tainted($^O);
    eval '$^O = $^X';
    test $@ =~ /Insecure dependency in/;
}

EFFECTIVELY_CONSTANTS: {
    my $tainted_number = 12 + $TAINT0;
    test tainted( $tainted_number );

    # Even though it's always 0, it's still tainted
    my $tainted_product = $tainted_number * 0;
    test tainted( $tainted_product );
    test $tainted_product == 0;
}

TERNARY_CONDITIONALS: {
    my $tainted_true  = $TAINT . "blah blah blah";
    my $tainted_false = $TAINT0;
    test tainted( $tainted_true );
    test tainted( $tainted_false );

    my $result = $tainted_true ? "True" : "False";
    test $result eq "True";
    test !tainted( $result );

    $result = $tainted_false ? "True" : "False";
    test $result eq "False";
    test !tainted( $result );

    my $untainted_whatever = "The Fabulous Johnny Cash";
    my $tainted_whatever = "Soft Cell" . $TAINT;

    $result = $tainted_true ? $tainted_whatever : $untainted_whatever;
    test $result eq "Soft Cell";
    test tainted( $result );

    $result = $tainted_false ? $tainted_whatever : $untainted_whatever;
    test $result eq "The Fabulous Johnny Cash";
    test !tainted( $result );
}

{
    # rt.perl.org 5900  $1 remains tainted if...
    # 1) The regular expression contains a scalar variable AND
    # 2) The regular expression appears in an elsif clause

    my $foo = "abcdefghi" . $TAINT;

    my $valid_chars = 'a-z';
    if ( $foo eq '' ) {
    }
    elsif ( $foo =~ /([$valid_chars]+)/o ) {
        test not tainted $1;
    }

    if ( $foo eq '' ) {
    }
    elsif ( my @bar = $foo =~ /([$valid_chars]+)/o ) {
        test not any_tainted @bar;
    }
}
