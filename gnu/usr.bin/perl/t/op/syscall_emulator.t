#!/usr/bin/perl
#	$OpenBSD: syscall_emulator.t,v 1.1 2023/09/03 01:43:09 afresh1 Exp $	#

# Copyright (c) 2023 Andrew Hewus Fresh <afresh1@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

BEGIN {
    chdir 't' if -d 't';
    require "./test.pl";
    set_up_inc( qw(. ../lib lib ../dist/base/lib) );
}

use v5.36;

use File::Temp;
use POSIX qw< S_IRUSR S_IWUSR S_IRGRP S_IROTH O_CREAT O_WRONLY O_RDONLY >;

use constant {
    PROT_READ   => 0x01,
    MAP_PRIVATE => 0x0002,
    MAP_FAILED  => -1,
};

my $dir = File::Temp->newdir("syscall_emulator-XXXXXXXXX");
{
	local $ENV{PERL5LIB} = join ':', @INC;
	open(my $fh, '-|', $^X, "../utils/h2ph", '-d', $dir,
	    "/usr/include/sys/syscall.h") or die "h2ph: $!";
	note <$fh>;
	close($fh) or die $! ? "h2ph: $!" : "h2ph: $?";
	local @INC = ("$dir/usr/include", "$dir");
	require 'sys/syscall.ph';
}

my $filename = "test.txt";
my $file = "$dir/$filename";
my $fd;
my $out = "Hello World\n";
my $in = "\0" x 32;
my ($in_p, $in_v);
my $sb = "\0" x 4096;
my $st_mode;

my $perms = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;

plan tests => 17;

ok(!
    (($fd = syscall(SYS_open(), $file, O_CREAT|O_WRONLY, $perms)) < 0),
    "Opened $filename for write/create"
);
ok(!
    (syscall(SYS_write(), $fd, $out, length $out) <= 0),
    "Wrote out to $filename"
);
ok(!
    (syscall(SYS_close(), $fd) != 0),
    "closed $filename"
);


ok(!
    (syscall(SYS_stat(), $file, $sb) != 0),
    "stat $filename"
);

# fortunately st_mode is the first unsigned long in stat struct
$st_mode = unpack "L", $sb;

ok( ($st_mode & 0777) == ($perms & 0777),
    sprintf "new file %s has correct permissions (%o)",
        $filename, $st_mode & 0777
);

ok(!
    (($fd = syscall(SYS_open(), $file, O_RDONLY)) < 0),
    "Opened $filename for read"
);
ok(!
    (syscall(SYS_read(), $fd, $in, length $in) <= 0),
    "read from $filename"
);

$in = unpack 'Z*', $in;

ok( length($in) == length($out) && ($in eq $out),
    "Read written content from $filename"
);

ok(!
    (syscall(SYS_lseek(), $fd, 0, SEEK_SET) < 0),
    "lseek on fd"
);

ok(!
    (syscall(SYS_pread(), $fd, $in = "\0" x 32, 5, 3) < 0),
    "pread on fd"
);

$in = unpack 'Z*', $in;

ok( length($in) == 5 && ($in eq substr $out, 3, 5),
    "Read written content from $filename ($in)"
);

ok(!
    (syscall(SYS_lseek(), $fd, 0, SEEK_SET) < 0),
    "lseek on fd"
);

ok(!
    (syscall(SYS_lseek(), $fd, 0, SEEK_SET) < 0),
    "lseek on fd"
);

ok(!
    (($in_p = syscall(SYS_mmap(), undef, length($out), PROT_READ, MAP_PRIVATE,
        $fd, 0)) == MAP_FAILED),
    "mmap fd"
);

# From ingy's Pointer module
$in_v = unpack "p*", pack "L!", $in_p;

ok( length($in_v) == length($out) && ($in_v eq $out),
    "Read written content from $filename"
);

ok(!
    (syscall(SYS_munmap(), $in_p, length($out)) != 0),
    "munmap fd"
);

ok(!
    (syscall(SYS_close(), $fd) != 0),
    "closed $filename"
);
