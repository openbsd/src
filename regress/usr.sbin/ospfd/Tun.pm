# Copyright (c) 2014 Alexander Bluhm <bluhm@openbsd.org>
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

# Encapsulate tun interface handling into separate module.

use strict;
use warnings;

package Tun;
use parent 'Exporter';
our @EXPORT_OK = qw(opentun);

use Carp;
use Fcntl;
use File::Basename;
use POSIX qw(_exit);
use PassFd 'recvfd';
use Socket;

sub opentun {
    my ($tun_number) = @_;
    my $tun_device = "/dev/tun$tun_number";

    if ($> == 0) {
	sysopen(my $tun, $tun_device, O_RDWR)
	    or croak "Open $tun_device failed: $!";
	return $tun;
    }

    if (!$ENV{SUDO}) {
	die "To open the device $tun_device you must run as root or\n".
	    "set the SUDO environment variable and allow closefrom_override.\n";
    }

    my $opentun;
    my $curdir = dirname($0) || ".";
    if (-x "$curdir/opentun") {
	$opentun = "$curdir/opentun";
    } elsif (-x "./opentun") {
	$opentun = "./opentun";
    } else {
	die "To open the device $tun_device the tool opentun is needed.\n".
	    "Executable opentun not found in $curdir or current directory.\n";
    }

    socketpair(my $parent, my $child, AF_UNIX, SOCK_STREAM, PF_UNSPEC)
	or croak "Socketpair failed: $!";
    $child->fcntl(F_SETFD, 0)
	or croak "Fcntl setfd failed: $!";

    defined(my $pid = fork())
	or croak "Fork failed: $!";

    unless ($pid) {
	# child process
	close($parent) or do {
	    warn "Close parent socket failed: $!";
	    _exit(3);
	};
	my @cmd = ($ENV{SUDO}, '-C', $child->fileno()+1, $opentun,
	    $child->fileno(), $tun_number);
	exec(@cmd);
	warn "Exec '@cmd' failed: $!";
	_exit(3);
    }

    # parent process
    close($child)
	or croak "Close child socket failed: $!";
    my $tun = recvfd($parent)
	or croak "Recvfd failed: $!";
    wait()
	or croak "Wait failed: $!";
    $? == 0
	or croak "Child process failed: $?";

    return $tun;
}

1;
