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
    @INC = '../lib' if -d '../lib';
}

use strict;
use Config;

my $Is_VMS = $^O eq 'VMS';
my $Is_MSWin32 = $^O eq 'MSWin32';
my $Invoke_Perl = $Is_VMS ? 'MCR Sys$Disk:[]Perl.' :
                  $Is_MSWin32 ? '.\perl' : './perl';
my @MoreEnv = qw/IFS CDPATH ENV BASH_ENV/;

if ($Is_VMS) {
    my (%old, $x);
    for $x ('DCL$PATH', @MoreEnv) {
	($old{$x}) = $ENV{$x} =~ /^(.*)$/ if exists $ENV{$x};
    }
    eval <<EndOfCleanup;
	END {
	    \$ENV{PATH} = '';
	    warn "# Note: logical name 'PATH' may have been deleted\n";
	    @ENV{keys %old} = values %old;
	}
EndOfCleanup
}

# Sources of taint:
#   The empty tainted value, for tainting strings
my $TAINT = substr($^X, 0, 0);
#   A tainted zero, useful for tainting numbers
my $TAINT0 = 0 + $TAINT;

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

sub test ($$;$) {
    my($serial, $boolean, $diag) = @_;
    if ($boolean) {
	print "ok $serial\n";
    } else {
	print "not ok $serial\n";
	for (split m/^/m, $diag) {
	    print "# $_";
	}
	print "\n" unless
	    $diag eq ''
	    or substr($diag, -1) eq "\n";
    }
}

# We need an external program to call.
my $ECHO = ($Is_MSWin32 ? ".\\echo$$" : "./echo$$");
END { unlink $ECHO }
open PROG, "> $ECHO" or die "Can't create $ECHO: $!";
print PROG 'print "@ARGV\n"', "\n";
close PROG;
my $echo = "$Invoke_Perl $ECHO";

print "1..140\n";

# First, let's make sure that Perl is checking the dangerous
# environment variables. Maybe they aren't set yet, so we'll
# taint them ourselves.
{
    $ENV{'DCL$PATH'} = '' if $Is_VMS;

    $ENV{PATH} = '';
    delete @ENV{@MoreEnv};
    $ENV{TERM} = 'dumb';

    test 1, eval { `$echo 1` } eq "1\n";

    if ($Is_MSWin32 || $Is_VMS) {
	print "# Environment tainting tests skipped\n";
	for (2..5) { print "ok $_\n" }
    }
    else {
	my @vars = ('PATH', @MoreEnv);
	while (my $v = $vars[0]) {
	    local $ENV{$v} = $TAINT;
	    last if eval { `$echo 1` };
	    last unless $@ =~ /^Insecure \$ENV{$v}/;
	    shift @vars;
	}
	test 2, !@vars, "\$$vars[0]";

	# tainted $TERM is unsafe only if it contains metachars
	local $ENV{TERM};
	$ENV{TERM} = 'e=mc2';
	test 3, eval { `$echo 1` } eq "1\n";
	$ENV{TERM} = 'e=mc2' . $TAINT;
	test 4, eval { `$echo 1` } eq '';
	test 5, $@ =~ /^Insecure \$ENV{TERM}/, $@;
    }

    my $tmp;
    if ($^O eq 'os2' || $^O eq 'amigaos' || $Is_MSWin32) {
	print "# all directories are writeable\n";
    }
    else {
	$tmp = (grep { defined and -d and (stat _)[2] & 2 }
		     qw(/tmp /var/tmp /usr/tmp /sys$scratch),
		     @ENV{qw(TMP TEMP)})[0]
	    or print "# can't find world-writeable directory to test PATH\n";
    }

    if ($tmp) {
	local $ENV{PATH} = $tmp;
	test 6, eval { `$echo 1` } eq '';
	test 7, $@ =~ /^Insecure directory in \$ENV{PATH}/, $@;
    }
    else {
	for (6..7) { print "ok $_\n" }
    }

    if ($Is_VMS) {
	$ENV{'DCL$PATH'} = $TAINT;
	test 8,  eval { `$echo 1` } eq '';
	test 9, $@ =~ /^Insecure \$ENV{DCL\$PATH}/, $@;
	if ($tmp) {
	    $ENV{'DCL$PATH'} = $tmp;
	    test 10, eval { `$echo 1` } eq '';
	    test 11, $@ =~ /^Insecure directory in \$ENV{DCL\$PATH}/, $@;
	}
	else {
	    print "# can't find world-writeable directory to test DCL\$PATH\n";
	    for (10..11) { print "ok $_\n" }
	}
	$ENV{'DCL$PATH'} = '';
    }
    else {
	print "# This is not VMS\n";
	for (8..11) { print "ok $_\n"; }
    }
}

# Let's see that we can taint and untaint as needed.
{
    my $foo = $TAINT;
    test 12, tainted $foo;

    # That was a sanity check. If it failed, stop the insanity!
    die "Taint checks don't seem to be enabled" unless tainted $foo;

    $foo = "foo";
    test 13, not tainted $foo;

    taint_these($foo);
    test 14, tainted $foo;

    my @list = 1..10;
    test 15, not any_tainted @list;
    taint_these @list[1,3,5,7,9];
    test 16, any_tainted @list;
    test 17, all_tainted @list[1,3,5,7,9];
    test 18, not any_tainted @list[0,2,4,6,8];

    ($foo) = $foo =~ /(.+)/;
    test 19, not tainted $foo;

    $foo = $1 if ('bar' . $TAINT) =~ /(.+)/;
    test 20, not tainted $foo;
    test 21, $foo eq 'bar';

    my $pi = 4 * atan2(1,1) + $TAINT0;
    test 22, tainted $pi;

    ($pi) = $pi =~ /(\d+\.\d+)/;
    test 23, not tainted $pi;
    test 24, sprintf("%.5f", $pi) eq '3.14159';
}

# How about command-line arguments? The problem is that we don't
# always get some, so we'll run another process with some.
{
    my $arg = "./arg$$";
    open PROG, "> $arg" or die "Can't create $arg: $!";
    print PROG q{
	eval { join('', @ARGV), kill 0 };
	exit 0 if $@ =~ /^Insecure dependency/;
	print "# Oops: \$@ was [$@]\n";
	exit 1;
    };
    close PROG;
    print `$Invoke_Perl "-T" $arg and some suspect arguments`;
    test 25, !$?, "Exited with status $?";
    unlink $arg;
}

# Reading from a file should be tainted
{
    my $file = './TEST';
    test 26, open(FILE, $file), "Couldn't open '$file': $!";

    my $block;
    sysread(FILE, $block, 100);
    my $line = <FILE>;
    close FILE;
    test 27, tainted $block;
    test 28, tainted $line;
}

# Globs should be forbidden, except under VMS,
#   which doesn't spawn an external program.
if ($Is_VMS) {
    for (29..30) { print "ok $_\n"; }
}
else {
    my @globs = eval { <*> };
    test 29, @globs == 0 && $@ =~ /^Insecure dependency/;

    @globs = eval { glob '*' };
    test 30, @globs == 0 && $@ =~ /^Insecure dependency/;
}

# Output of commands should be tainted
{
    my $foo = `$echo abc`;
    test 31, tainted $foo;
}

# Certain system variables should be tainted
{
    test 32, all_tainted $^X, $0;
}

# Results of matching should all be untainted
{
    my $foo = "abcdefghi" . $TAINT;
    test 33, tainted $foo;

    $foo =~ /def/;
    test 34, not any_tainted $`, $&, $';

    $foo =~ /(...)(...)(...)/;
    test 35, not any_tainted $1, $2, $3, $+;

    my @bar = $foo =~ /(...)(...)(...)/;
    test 36, not any_tainted @bar;

    test 37, tainted $foo;	# $foo should still be tainted!
    test 38, $foo eq "abcdefghi";
}

# Operations which affect files can't use tainted data.
{
    test 39, eval { chmod 0, $TAINT } eq '', 'chmod';
    test 40, $@ =~ /^Insecure dependency/, $@;

    # There is no feature test in $Config{} for truncate,
    #   so we allow for the possibility that it's missing.
    test 41, eval { truncate 'NoSuChFiLe', $TAINT0 } eq '', 'truncate';
    test 42, $@ =~ /^(?:Insecure dependency|truncate not implemented)/, $@;

    test 43, eval { rename '', $TAINT } eq '', 'rename';
    test 44, $@ =~ /^Insecure dependency/, $@;

    test 45, eval { unlink $TAINT } eq '', 'unlink';
    test 46, $@ =~ /^Insecure dependency/, $@;

    test 47, eval { utime $TAINT } eq '', 'utime';
    test 48, $@ =~ /^Insecure dependency/, $@;

    if ($Config{d_chown}) {
	test 49, eval { chown -1, -1, $TAINT } eq '', 'chown';
	test 50, $@ =~ /^Insecure dependency/, $@;
    }
    else {
	print "# chown() is not available\n";
	for (49..50) { print "ok $_\n" }
    }

    if ($Config{d_link}) {
	test 51, eval { link $TAINT, '' } eq '', 'link';
	test 52, $@ =~ /^Insecure dependency/, $@;
    }
    else {
	print "# link() is not available\n";
	for (51..52) { print "ok $_\n" }
    }

    if ($Config{d_symlink}) {
	test 53, eval { symlink $TAINT, '' } eq '', 'symlink';
	test 54, $@ =~ /^Insecure dependency/, $@;
    }
    else {
	print "# symlink() is not available\n";
	for (53..54) { print "ok $_\n" }
    }
}

# Operations which affect directories can't use tainted data.
{
    test 55, eval { mkdir $TAINT0, $TAINT } eq '', 'mkdir';
    test 56, $@ =~ /^Insecure dependency/, $@;

    test 57, eval { rmdir $TAINT } eq '', 'rmdir';
    test 58, $@ =~ /^Insecure dependency/, $@;

    test 59, eval { chdir $TAINT } eq '', 'chdir';
    test 60, $@ =~ /^Insecure dependency/, $@;

    if ($Config{d_chroot}) {
	test 61, eval { chroot $TAINT } eq '', 'chroot';
	test 62, $@ =~ /^Insecure dependency/, $@;
    }
    else {
	print "# chroot() is not available\n";
	for (61..62) { print "ok $_\n" }
    }
}

# Some operations using files can't use tainted data.
{
    my $foo = "imaginary library" . $TAINT;
    test 63, eval { require $foo } eq '', 'require';
    test 64, $@ =~ /^Insecure dependency/, $@;

    my $filename = "./taintB$$";	# NB: $filename isn't tainted!
    END { unlink $filename if defined $filename }
    $foo = $filename . $TAINT;
    unlink $filename;	# in any case

    test 65, eval { open FOO, $foo } eq '', 'open for read';
    test 66, $@ eq '', $@;		# NB: This should be allowed
    test 67, $! == 2;			# File not found

    test 68, eval { open FOO, "> $foo" } eq '', 'open for write';
    test 69, $@ =~ /^Insecure dependency/, $@;
}

# Commands to the system can't use tainted data
{
    my $foo = $TAINT;

    if ($^O eq 'amigaos') {
	print "# open(\"|\") is not available\n";
	for (70..73) { print "ok $_\n" }
    }
    else {
	test 70, eval { open FOO, "| $foo" } eq '', 'popen to';
	test 71, $@ =~ /^Insecure dependency/, $@;

	test 72, eval { open FOO, "$foo |" } eq '', 'popen from';
	test 73, $@ =~ /^Insecure dependency/, $@;
    }

    test 74, eval { exec $TAINT } eq '', 'exec';
    test 75, $@ =~ /^Insecure dependency/, $@;

    test 76, eval { system $TAINT } eq '', 'system';
    test 77, $@ =~ /^Insecure dependency/, $@;

    $foo = "*";
    taint_these $foo;

    test 78, eval { `$echo 1$foo` } eq '', 'backticks';
    test 79, $@ =~ /^Insecure dependency/, $@;

    if ($Is_VMS) { # wildcard expansion doesn't invoke shell, so is safe
	test 80, join('', eval { glob $foo } ) ne '', 'globbing';
	test 81, $@ eq '', $@;
    }
    else {
	for (80..81) { print "ok $_\n"; }
    }
}

# Operations which affect processes can't use tainted data.
{
    test 82, eval { kill 0, $TAINT } eq '', 'kill';
    test 83, $@ =~ /^Insecure dependency/, $@;

    if ($Config{d_setpgrp}) {
	test 84, eval { setpgrp 0, $TAINT } eq '', 'setpgrp';
	test 85, $@ =~ /^Insecure dependency/, $@;
    }
    else {
	print "# setpgrp() is not available\n";
	for (84..85) { print "ok $_\n" }
    }

    if ($Config{d_setprior}) {
	test 86, eval { setpriority 0, $TAINT, $TAINT } eq '', 'setpriority';
	test 87, $@ =~ /^Insecure dependency/, $@;
    }
    else {
	print "# setpriority() is not available\n";
	for (86..87) { print "ok $_\n" }
    }
}

# Some miscellaneous operations can't use tainted data.
{
    if ($Config{d_syscall}) {
	test 88, eval { syscall $TAINT } eq '', 'syscall';
	test 89, $@ =~ /^Insecure dependency/, $@;
    }
    else {
	print "# syscall() is not available\n";
	for (88..89) { print "ok $_\n" }
    }

    {
	my $foo = "x" x 979;
	taint_these $foo;
	local *FOO;
	my $temp = "./taintC$$";
	END { unlink $temp }
	test 90, open(FOO, "> $temp"), "Couldn't open $temp for write: $!";

	test 91, eval { ioctl FOO, $TAINT, $foo } eq '', 'ioctl';
	test 92, $@ =~ /^Insecure dependency/, $@;

	if ($Config{d_fcntl}) {
	    test 93, eval { fcntl FOO, $TAINT, $foo } eq '', 'fcntl';
	    test 94, $@ =~ /^Insecure dependency/, $@;
	}
	else {
	    print "# fcntl() is not available\n";
	    for (93..94) { print "ok $_\n" }
	}

	close FOO;
    }
}

# Some tests involving references
{
    my $foo = 'abc' . $TAINT;
    my $fooref = \$foo;
    test 95, not tainted $fooref;
    test 96, tainted $$fooref;
    test 97, tainted $foo;
}

# Some tests involving assignment
{
    my $foo = $TAINT0;
    my $bar = $foo;
    test 98, all_tainted $foo, $bar;
    test 99, tainted($foo = $bar);
    test 100, tainted($bar = $bar);
    test 101, tainted($bar += $bar);
    test 102, tainted($bar -= $bar);
    test 103, tainted($bar *= $bar);
    test 104, tainted($bar++);
    test 105, tainted($bar /= $bar);
    test 106, tainted($bar += 0);
    test 107, tainted($bar -= 2);
    test 108, tainted($bar *= -1);
    test 109, tainted($bar /= 1);
    test 110, tainted($bar--);
    test 111, $bar == 0;
}

# Test assignment and return of lists
{
    my @foo = ("A", "tainted" . $TAINT, "B");
    test 112, not tainted $foo[0];
    test 113,     tainted $foo[1];
    test 114, not tainted $foo[2];
    my @bar = @foo;
    test 115, not tainted $bar[0];
    test 116,     tainted $bar[1];
    test 117, not tainted $bar[2];
    my @baz = eval { "A", "tainted" . $TAINT, "B" };
    test 118, not tainted $baz[0];
    test 119,     tainted $baz[1];
    test 120, not tainted $baz[2];
    my @plugh = eval q[ "A", "tainted" . $TAINT, "B" ];
    test 121, not tainted $plugh[0];
    test 122,     tainted $plugh[1];
    test 123, not tainted $plugh[2];
    my $nautilus = sub { "A", "tainted" . $TAINT, "B" };
    test 124, not tainted ((&$nautilus)[0]);
    test 125,     tainted ((&$nautilus)[1]);
    test 126, not tainted ((&$nautilus)[2]);
    my @xyzzy = &$nautilus;
    test 127, not tainted $xyzzy[0];
    test 128,     tainted $xyzzy[1];
    test 129, not tainted $xyzzy[2];
    my $red_october = sub { return "A", "tainted" . $TAINT, "B" };
    test 130, not tainted ((&$red_october)[0]);
    test 131,     tainted ((&$red_october)[1]);
    test 132, not tainted ((&$red_october)[2]);
    my @corge = &$red_october;
    test 133, not tainted $corge[0];
    test 134,     tainted $corge[1];
    test 135, not tainted $corge[2];
}

# Test for system/library calls returning string data of dubious origin.
{
    # No reliable %Config check for getpw*
    if (eval { setpwent(); getpwent(); 1 }) {
	setpwent();
	my @getpwent = getpwent();
	die "getpwent: $!\n" unless (@getpwent);
	test 136,(    not tainted $getpwent[0]
	          and not tainted $getpwent[1]
	          and not tainted $getpwent[2]
	          and not tainted $getpwent[3]
	          and not tainted $getpwent[4]
	          and not tainted $getpwent[5]
	          and     tainted $getpwent[6] # gecos
	          and not tainted $getpwent[7]
		  and not tainted $getpwent[8]);
	endpwent();
    } else {
	print "# getpwent() is not available\n";
	print "ok 136\n";
    }

    if ($Config{d_readdir}) { # pretty hard to imagine not
	local(*D);
	opendir(D, "op") or die "opendir: $!\n";
	my $readdir = readdir(D);
	test 137, tainted $readdir;
	closedir(OP);
    } else {
	print "# readdir() is not available\n";
	print "ok 137\n";
    }

    if ($Config{d_readlink} && $Config{d_symlink}) {
	my $symlink = "sl$$";
	unlink($symlink);
	symlink("/something/naughty", $symlink) or die "symlink: $!\n";
	my $readlink = readlink($symlink);
	test 138, tainted $readlink;
	unlink($symlink);
    } else {
	print "# readlink() or symlink() is not available\n";
	print "ok 138\n";
    }
}

# test bitwise ops (regression bug)
{
    my $why = "y";
    my $j = "x" | $why;
    test 139, not tainted $j;
    $why = $TAINT."y";
    $j = "x" | $why;
    test 140,     tainted $j;
}

