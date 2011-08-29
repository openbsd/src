#	$OpenBSD: funcs.pl,v 1.8 2011/08/29 01:50:38 bluhm Exp $

# Copyright (c) 2010,2011 Alexander Bluhm <bluhm@openbsd.org>
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

use strict;
use warnings;
use feature 'switch';
use Errno;
use Digest::MD5;
use IO::Socket qw(sockatmark);
use Socket;
use Time::HiRes qw(time alarm sleep);
use BSD::Socket::Splice qw(setsplice getsplice geterror);

########################################################################
# Client funcs
########################################################################

sub write_char {
	my $self = shift;
	my $len = shift // $self->{len} // 251;
	my $sleep = $self->{sleep};

	my $ctx = Digest::MD5->new();
	my $char = '0';
	for (my $i = 1; $i < $len; $i++) {
		$ctx->add($char);
		print $char
		    or die ref($self), " print failed: $!";
		given ($char) {
			when(/9/)	{ $char = 'A' }
			when(/Z/)	{ $char = 'a' }
			when(/z/)	{ $char = "\n" }
			when(/\n/)	{ print STDERR "."; $char = '0' }
			default		{ $char++ }
		}
		if ($self->{sleep}) {
			IO::Handle::flush(\*STDOUT);
			sleep $self->{sleep};
		}
	}
	if ($len) {
		$char = "\n";
		$ctx->add($char);
		print $char
		    or die ref($self), " print failed: $!";
		print STDERR ".\n";
	}
	IO::Handle::flush(\*STDOUT);

	print STDERR "LEN: ", $len, "\n";
	print STDERR "MD5: ", $ctx->hexdigest, "\n";
}

sub write_oob {
	my $self = shift;
	my $len = shift // $self->{len} // 251;

	my $ctx = Digest::MD5->new();
	my $msg = "";
	my $char = '0';
	for (my $i = 1; $i < $len; $i++) {
		$msg .= $char;
		given ($char) {
			when(/9/) {
				$ctx->add("[$char]");
				defined(send(STDOUT, $msg, MSG_OOB))
				    or die ref($self), " send OOB failed: $!";
				# If tcp urgent data is sent too fast,
				# it may get overwritten and lost.
				sleep .1;
				$msg = "";
				$char = 'A';
			}
			when(/Z/)	{ $ctx->add($char); $char = 'a' }
			when(/z/)	{ $ctx->add($char); $char = "\n" }
			when(/\n/) {
				$ctx->add($char);
				defined(send(STDOUT, $msg, 0))
				    or die ref($self), " send failed: $!";
				print STDERR ".";
				$msg = "";
				$char = '0';
			}
			default		{ $ctx->add($char); $char++ }
		}
	}
	if ($len) {
		$char = "\n";
		$msg .= $char;
		$ctx->add($char);
		send(STDOUT, $msg, 0)
		    or die ref($self), " send failed: $!";
		print STDERR ".\n";
	}
	IO::Handle::flush(\*STDOUT);

	print STDERR "LEN: ", $len, "\n";
	print STDERR "MD5: ", $ctx->hexdigest, "\n";
}

########################################################################
# Relay funcs
########################################################################

sub relay_copy {
	my $self = shift;
	my $max = $self->{max};
	my $idle = $self->{idle};
	my $size = $self->{size} || 8093;

	my $len = 0;
	while (1) {
		my $rin = my $win = my $ein = '';
		vec($rin, fileno(STDIN), 1) = 1;
		vec($ein, fileno(STDIN), 1) = 1 unless $self->{oobinline};
		defined(my $n = select($rin, undef, $ein, $idle))
		    or die ref($self), " select failed: $!";
		if ($idle && $n == 0) {
			print STDERR "\n";
			print STDERR "Timeout\n";
			last;
		}
		my $buf;
		my $atmark = sockatmark(\*STDIN)
		    or die ref($self), " sockatmark failed: $!";
		if ($atmark == 1) {
			if ($self->{oobinline}) {
				defined(recv(STDIN, $buf, 1, 0))
				    or die ref($self), " recv OOB failed: $!";
				$len += length($buf);
				defined(send(STDOUT, $buf, MSG_OOB))
				    or die ref($self), " send OOB failed: $!";
			} else {
				defined(recv(STDIN, $buf, 1, MSG_OOB)) ||
				    $!{EINVAL}
				    or die ref($self), " recv OOB failed: $!";
				print STDERR "OOB: $buf\n" if length($buf);
			}
		}
		if ($self->{nonblocking}) {
			vec($rin, fileno(STDIN), 1) = 1;
			select($rin, undef, undef, undef)
			    or die ref($self), " select read failed: $!";
		}
		my $read = sysread(STDIN, $buf,
		    $max && $max < $size ? $max : $size);
		defined($read)
		    or die ref($self), " sysread at $len failed: $!";
		if ($read == 0) {
			print STDERR "\n";
			print STDERR "End\n";
			last;
		}
		print STDERR ".";
		if ($max && $len + $read > $max) {
			$read = $max - $len;
		}
		my $off = 0;
		while ($off < $read) {
			if ($self->{nonblocking}) {
				vec($win, fileno(STDOUT), 1) = 1;
				select(undef, $win, undef, undef)
				    or die ref($self),
				    " select write failed: $!";
			}
			my $write = syswrite(STDOUT, $buf, $read - $off, $off);
			defined($write) || $!{ETIMEDOUT}
			    or die ref($self), " syswrite at $len failed: $!";
			defined($write) or next;
			$off += $write;
			$len += $write;
		}
		if ($max && $len == $max) {
			print STDERR "\n";
			print STDERR "Max\n";
			last;
		}
	}

	print STDERR "LEN: ", $len, "\n";
}

sub relay_splice {
	my $self = shift;
	my $max = $self->{max};
	my $idle = $self->{idle};

	my $len = 0;
	my $splicelen;
	my $shortsplice = 0;
	my $error;
	do {
		my $splicemax = $max ? $max - $len : 0;
		setsplice(\*STDIN, \*STDOUT, $splicemax, $idle)
		    or die ref($self), " splice stdin to stdout failed: $!";

		if ($self->{readblocking}) {
			my $read;
			# block by reading from the source socket
			do {
				# busy loop to test soreceive
				$read = sysread(STDIN, my $buf, 2**16);
			} while ($self->{nonblocking} && !defined($read) &&
			    $!{EAGAIN});
			defined($read)
			    or die ref($self), " read blocking failed: $!";
			$read > 0 and die ref($self),
			    " read blocking has data: $read";
			print STDERR "Read\n";
		} else {
			my $rin = '';
			vec($rin, fileno(STDIN), 1) = 1;
			select($rin, undef, undef, undef)
			    or die ref($self), " select failed: $!";
		}

		defined($error = geterror(\*STDIN))
		    or die ref($self), " get error from stdin failed: $!";
		($! = $error) && ! $!{ETIMEDOUT}
		    and die ref($self), " splice failed: $!";

		defined($splicelen = getsplice(\*STDIN))
		    or die ref($self), " get splice len from stdin failed: $!";
		print STDERR "SPLICELEN: ", $splicelen, "\n";
		!$max || $splicelen <= $splicemax
		    or die ref($self), " splice len $splicelen ".
		    "greater than max $splicemax";
		$len += $splicelen;
	} while ($max && $max > $len && !$shortsplice++);

	if ($idle && $error == Errno::ETIMEDOUT) {
		print STDERR "Timeout\n";
	}
	if ($max && $max == $len) {
		print STDERR "Max\n";
	} elsif ($max && $max < $len) {
		die ref($self), " max $max less than len $len";
	} elsif ($max && $max > $len && $splicelen) {
		die ref($self), " max $max greater than len $len";
	} elsif (!$error) {
		defined(my $read = sysread(STDIN, my $buf, 2**16))
		    or die ref($self), " sysread stdin failed: $!";
		$read > 0
		    and die ref($self), " sysread stdin has data: $read";
		print STDERR "End\n";
	}
	print STDERR "LEN: ", $len, "\n";
}

sub relay {
	my $self = shift;
	my $forward = $self->{forward};

	given ($forward) {
		when (/splice/)	{ relay_splice($self, @_) }
		when (/copy/)	{ relay_copy($self, @_) }
		default		{ die "Unknown forward name: $forward" }
	}

	my $soerror;
	$soerror = getsockopt(STDIN, SOL_SOCKET, SO_ERROR)
	    or die ref($self), " get error from stdin failed: $!";
	print STDERR "ERROR IN: ", unpack('i', $soerror), "\n";
	$soerror = getsockopt(STDOUT, SOL_SOCKET, SO_ERROR)
	    or die ref($self), " get error from stdout failed: $!";
	print STDERR "ERROR OUT: ", unpack('i', $soerror), "\n";
}

sub ioflip {
	my $self = shift;

	open(my $fh, '<&', \*STDIN)
	    or die ref($self), " ioflip dup failed: $!";
	open(STDIN, '<&', \*STDOUT)
	    or die ref($self), " ioflip dup STDIN failed: $!";
	open(STDOUT, '>&', $fh)
	    or die ref($self), " ioflip dup STDOUT failed: $!";
	close($fh)
	    or die ref($self), " ioflip close failed: $!";
}

sub errignore {
	$SIG{PIPE} = 'IGNORE';
	$SIG{__DIE__} = sub {
		die @_ if $^S;
		warn @_;
		my $soerror;
		$soerror = getsockopt(STDIN, SOL_SOCKET, SO_ERROR);
		print STDERR "ERROR IN: ", unpack('i', $soerror), "\n";
		$soerror = getsockopt(STDOUT, SOL_SOCKET, SO_ERROR);
		print STDERR "ERROR OUT: ", unpack('i', $soerror), "\n";
		IO::Handle::flush(\*STDERR);
		POSIX::_exit(0);
	};
}

sub shutin {
	my $self = shift;
	shutdown(\*STDIN, SHUT_RD)
	    or die ref($self), " shutdown read failed: $!";
}

sub shutout {
	my $self = shift;
	IO::Handle::flush(\*STDOUT)
	    or die ref($self), " flush stdout failed: $!";
	shutdown(\*STDOUT, SHUT_WR)
	    or die ref($self), " shutdown write failed: $!";
}

########################################################################
# Server funcs
########################################################################

sub read_char {
	my $self = shift;
	my $max = $self->{max};

	my $ctx = Digest::MD5->new();
	my $len = 0;
	while (<STDIN>) {
		$len += length($_);
		$ctx->add($_);
		print STDERR ".";
		if ($max && $len >= $max) {
			print STDERR "\nMax";
			last;
		}
	}
	print STDERR "\n";

	print STDERR "LEN: ", $len, "\n";
	print STDERR "MD5: ", $ctx->hexdigest, "\n";
}

sub read_oob {
	my $self = shift;
	my $size = $self->{size} || 4091;

	my $ctx = Digest::MD5->new();
	my $len = 0;
	while (1) {
		my $rin = my $ein = '';
		vec($rin, fileno(STDIN), 1) = 1;
		vec($ein, fileno(STDIN), 1) = 1 unless $self->{oobinline};
		select($rin, undef, $ein, undef)
		    or die ref($self), " select failed: $!";
		my $buf;
		my $atmark = sockatmark(\*STDIN)
		    or die ref($self), " sockatmark failed: $!";
		if ($atmark == 1) {
			if ($self->{oobinline}) {
				defined(recv(STDIN, $buf, 1, 0))
				    or die ref($self), " recv OOB failed: $!";
				print STDERR "[$buf]";
				$ctx->add("[$buf]");
				$len += length($buf);
			} else {
				defined(recv(STDIN, $buf, 1, MSG_OOB)) ||
				    $!{EINVAL}
				    or die ref($self), " recv OOB failed: $!";
				print STDERR "OOB: $buf\n" if length($buf);
			}
		}
		defined(recv(STDIN, $buf, $size, 0))
		    or die ref($self), " recv failed: $!";
		last unless length($buf);
		print STDERR $buf;
		$ctx->add($buf);
		$len += length($buf);
		print STDERR ".";
	}
	print STDERR "\n";

	print STDERR "LEN: ", $len, "\n";
	print STDERR "MD5: ", $ctx->hexdigest, "\n";
}

sub solinger {
	my $self = shift;

	setsockopt(STDIN, SOL_SOCKET, SO_LINGER, pack('ii', 1, 0))
	    or die ref($self), " set linger failed: $!";
}

1;
