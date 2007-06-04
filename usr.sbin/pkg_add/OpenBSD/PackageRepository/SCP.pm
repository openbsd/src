# ex:ts=8 sw=4:
# $OpenBSD: SCP.pm,v 1.15 2007/06/04 18:52:02 espie Exp $
#
# Copyright (c) 2003-2006 Marc Espie <espie@openbsd.org>
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

package OpenBSD::PackageRepository::SCP;
our @ISA=qw(OpenBSD::PackageRepository::Distant);

use IPC::Open2;
use IO::Handle;

sub urlscheme
{
	return 'scp';
}

our %distant = ();

# Any SCP repository uses one single connection, reliant on a perl at end.
# The connection starts by xfering and firing up the `distant' script.
sub initiate
{
	my $self = shift;

	my ($rdfh, $wrfh);

	$self->{controller} = open2($rdfh, $wrfh, 'ssh', $self->{host}, 'perl', '-x');
	$self->{cmdfh} = $wrfh;
	$self->{getfh} = $rdfh;
	$wrfh->autoflush(1);
	local $_;

	while(<DATA>) {
		# compress script a bit
		next if m/^\#/o && !m/^\#!/o;
		s/^\s*//o;
		next if m/^$/o;
		print $wrfh $_;
	}
}
	
	
sub may_exist
{
	my ($self, $name) = @_;
	my $l = $self->list;
	return grep {$_ eq $name } @$l;
}

sub grab_object
{
	my ($self, $object) = @_;

	my $cmdfh = $self->{cmdfh};
	my $getfh = $self->{getfh};

	print $cmdfh "ABORT\n";
	local $_;
	while (<$getfh>) {
		last if m/^ABORTED/o;
	}
	print $cmdfh "GET ", $self->{path}.$object->{name}.".tgz", "\n";
	close($cmdfh);
	$_ = <$getfh>;
	chomp;
	if (m/^ERROR:/o) {
		die "transfer error: $_";
	}
	if (m/^TRANSFER:\s+(\d+)/o) {
		my $buffsize = 10 * 1024;
		my $buffer;
		my $size = $1;
		my $remaining = $size;
		my $n;

		do {
			$n = read($getfh, $buffer, 
				$remaining < $buffsize ? $remaining :$buffsize);
			if (!defined $n) {
				die "Error reading\n";
			}
			$remaining -= $n;
			if ($n > 0) {
				syswrite STDOUT, $buffer;
			}
		} while ($n != 0 && $remaining != 0);
		exit(0);
	}
}

sub _new
{
	my ($class, $baseurl) = @_;
	if ($baseurl =~ m/^\/\/(.*?)(\/.*)$/o) {
		bless {	host => $1, baseurl => $baseurl, 
		    key => $1, path => $2 }, $class;
	} else {
		die "Invalid scp url: scp:$baseurl\n";
	}
}

sub maxcount
{
	return 1;
}

sub opened
{
	my $self = $_[0];
	my $k = $self->{key};
	if (!defined $distant{$k}) {
		$distant{$k} = [];
	}
	return $distant{$k};
}

sub list
{
	my ($self) = @_;
	if (!defined $self->{list}) {
		if (!defined $self->{controller}) {
			$self->initiate;
		}
		my $cmdfh = $self->{cmdfh};
		my $getfh = $self->{getfh};
		my $path = $self->{path};
		my $l = [];
		print $cmdfh "LIST $path\n";
		local $_;
		$_ = <$getfh>;
		if (!defined $_) {
			die "Could not initiate SSH session\n";
		}
		chomp;
		if (m/^ERROR:/o) {
			die $_;
		}
		if (!m/^SUCCESS:/o) {
			die "Synchronization error\n";
		}
		while (<$getfh>) {
			chomp;
			last if $_ eq '';
			push(@$l, $_);
		}
		$self->{list} = $l;
	}
	return $self->{list};
}

sub cleanup
{
	my $self = shift;
	if (defined $self->{controller}) {
		my $cmdfh = $self->{cmdfh};
		my $getfh = $self->{getfh};
		print $cmdfh "ABORT\nBYE\nBYE\n";
		CORE::close($cmdfh);
		CORE::close($getfh);
		waitpid($self->{controller}, 0);
	}
}

1;
__DATA__
# Distant connection script.
#! /usr/bin/perl

my $pid;
my $token = 0;
$|= 1;

sub batch(&)
{
	my $code = shift;
	if (defined $pid) {
		waitpid($pid, 0);
		undef $pid;
	}
	$token++;
	$pid = fork();
	if (!defined $pid) {
		print "ERROR: fork failed: $!\n";
	}
	if ($pid == 0) {
		&$code();
		exit(0);
	}
}

sub abort_batch()
{
	if (defined $pid) {
		kill 1, $pid;
		waitpid($pid, 0);
		undef $pid;
	}
	print "\nABORTED $token\n";
}


local $_;
while (<STDIN>) {
	chomp;
	if (m/^LIST\s+(.*)$/o) {
		my $dname = $1;
		batch(sub {
			my $d;
			if (opendir($d, $dname)) {
				print "SUCCESS: directory $dname\n";
			} else {
				print "ERROR: bad directory $dname $!\n";
			}
			while (my $e = readdir($d)) {
				next if $e eq '.' or $e eq '..';
				next unless $e =~ m/(.+)\.tgz$/;
				next unless -f "$dname/$e";
				print "$1\n";
			}
			print "\n";
			closedir($d);
		});
	} elsif (m/^GET\s+(.*)$/o) {
		my $fname = $1;
		batch(sub {
			if (open(my $fh, '<', $fname)) {
				my $size = (stat $fh)[7];
				print "TRANSFER: $size\n";
				my $buffer = '';
				while (read($fh, $buffer, 1024 * 1024) > 0) {
					print $buffer;
				}
				close($fh);
			} else {
				print "ERROR: bad file $fname $!\n";
			}
		});
	} elsif (m/^BYE$/o) {
		exit(0);
	} elsif (m/^ABORT$/o) {
		abort_batch();
	} else {
		print "ERROR: Unknown command\n";
	}
}
__END__
