#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

# Script to test auto flush on fork/exec/system/qx.  The idea is to
# print "Pe" to a file from a parent process and "rl" to the same file
# from a child process.  If buffers are flushed appropriately, the
# file should contain "Perl".  We'll see...
use Config;
use warnings;
use strict;

# This attempts to mirror the #ifdef forest found in perl.h so that we
# know when to run these tests.  If that forest ever changes, change
# it here too or expect test gratuitous test failures.
my $useperlio = defined $Config{useperlio} ? $Config{useperlio} eq 'define' ? 1 : 0 : 0;
my $fflushNULL = defined $Config{fflushNULL} ? $Config{fflushNULL} eq 'define' ? 1 : 0 : 0;
my $d_sfio = defined $Config{d_sfio} ? $Config{d_sfio} eq 'define' ? 1 : 0 : 0;
my $fflushall = defined $Config{fflushall} ? $Config{fflushall} eq 'define' ? 1 : 0 : 0;
my $d_fork = defined $Config{d_fork} ? $Config{d_fork} eq 'define' ? 1 : 0 : 0;

if ($useperlio || $fflushNULL || $d_sfio) {
    print "1..4\n";
} else {
    if ($fflushall) {
	print "1..4\n";
    } else {
	print "1..0 # Skip: fflush(NULL) or equivalent not available\n";
        exit;
    }
}

my $runperl = qq{$^X "-I../lib"};
my @delete;

END {
    for (@delete) {
	unlink $_ or warn "unlink $_: $!";
    }
}

sub file_eq {
    my $f   = shift;
    my $val = shift;

    open IN, $f or die "open $f: $!";
    chomp(my $line = <IN>);
    close IN;

    print "# got $line\n";
    print "# expected $val\n";
    return $line eq $val;
}

# This script will be used as the command to execute from
# child processes
open PROG, "> ff-prog" or die "open ff-prog: $!";
print PROG <<'EOF';
my $f = shift;
my $str = shift;
open OUT, ">> $f" or die "open $f: $!";
print OUT $str;
close OUT;
EOF
    ;
close PROG or die "close ff-prog: $!";;
push @delete, "ff-prog";

$| = 0; # we want buffered output

# Test flush on fork/exec
if (!$d_fork) {
    print "ok 1 # skipped: no fork\n";
} else {
    my $f = "ff-fork-$$";
    open OUT, "> $f" or die "open $f: $!";
    print OUT "Pe";
    my $pid = fork;
    if ($pid) {
	# Parent
	wait;
	close OUT or die "close $f: $!";
    } elsif (defined $pid) {
	# Kid
	print OUT "r";
	my $command = qq{$runperl "ff-prog" "$f" "l"};
	print "# $command\n";
	exec $command or die $!;
	exit;
    } else {
	# Bang
	die "fork: $!";
    }

    print file_eq($f, "Perl") ? "ok 1\n" : "not ok 1\n";
    push @delete, $f;
}

# Test flush on system/qx/pipe open
my %subs = (
            "system" => sub {
                my $c = shift;
                system $c;
            },
            "qx"     => sub {
                my $c = shift;
                qx{$c};
            },
            "popen"  => sub {
                my $c = shift;
                open PIPE, "$c|" or die "$c: $!";
                close PIPE;
            },
            );
my $t = 2;
for (qw(system qx popen)) {
    my $code    = $subs{$_};
    my $f       = "ff-$_-$$";
    my $command = qq{$runperl "ff-prog" "$f" "rl"};
    open OUT, "> $f" or die "open $f: $!";
    print OUT "Pe";
    close OUT or die "close $f: $!";;
    print "# $command\n";
    $code->($command);
    print file_eq($f, "Perl") ? "ok $t\n" : "not ok $t\n";
    push @delete, $f;
    ++$t;
}
