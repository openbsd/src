# ex:ts=8 sw=4:
# $OpenBSD: Vstat.pm,v 1.49 2010/01/02 14:33:57 espie Exp $
#
# Copyright (c) 2003-2007 Marc Espie <espie@openbsd.org>
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

# Provides stat and statfs-like functions for package handling.

# allows user to add/remove files.

# uses mount and df directly for now.

use strict;
use warnings;

package OpenBSD::Vstat;
use File::Basename;
use OpenBSD::Paths;

sub stat
{
	my ($self, $fname) = @_;
	my $dev = (stat $fname)[0];

	if (!defined $dev && $fname ne '/') {
		return $self->stat(dirname($fname));
	}
	my $mp = OpenBSD::MountPoint->find($dev, $fname, $self->{state});
	if (!defined $self->{p}[0]{$mp}) {
		$self->{p}[0]{$mp} = OpenBSD::MountPoint::Proxy->new(0, $mp);
	}
	return $self->{p}[0]{$mp};
}

sub account_for
{
	my ($self, $name, $size) = @_;
	my $e = $self->stat($name);
	$e->{used} += $size;
	return $e;
}

sub account_later
{
	my ($self, $name, $size) = @_;
	my $e = $self->stat($name);
	$e->{delayed} += $size;
	return $e;
}

sub new
{
	my ($class, $state) = @_;

	bless {v => [{}], p => [{}], state => $state}, $class;
}

sub exists
{
	my ($self, $name) = @_;
	for my $v (@{$self->{v}}) {
		if (defined $v->{$name}) {
			return $v->{$name};
		}
	}
	return -e $name;
}

sub synchronize
{
	my ($self) = @_;
	for my $v (values %{$self->{p}[0]}) {
		$v->{used} += $v->{delayed};
		$v->{delayed} = 0;
	}
	return if $self->{state}->{not};
	$self->{v} = [{}];
}

sub add
{
	my ($self, $name, $size, $value) = @_;
	if (defined $value) {
		$self->{v}[0]->{$name} = $value;
	} else {
		$self->{v}[0]->{$name} = 1;
	}
	return defined($size) ? $self->account_for($name, $size) : undef;
}

sub remove
{
	my ($self, $name, $size) = @_;
	$self->{v}[0]->{$name} = 0;
	return defined($size) ? $self->account_later($name, -$size) : undef;
}

sub tally
{
	my $self = shift;

	for my $data (values %{$self->{p}[0]}) {
		if ($data->{used} != 0) {
			print $data->name, ": ", $data->{used}, " bytes";
			my $avail = $data->avail; 
			if ($avail < 0) {
				print " (missing ", int(-$avail+1), " blocks)";
			}
			print "\n";
		}
	}
}

package OpenBSD::MountPoint;

my $devinfo;
my $giveup;

sub parse_opts
{
	my ($self, $opts) = @_;
	for my $o (split /\,\s*/o, $opts) {
		if ($o eq 'read-only') {
			$self->{ro} = 1;
		} elsif ($o eq 'nodev') {
			$self->{nodev} = 1;
		} elsif ($o eq 'nosuid') {
			$self->{nosuid} = 1;
		} elsif ($o eq 'noexec') {
			$self->{noexec} = 1;
		}
	}
}

sub ro
{
	return shift->{ro};
}

sub nodev
{
	return shift->{nodev};
}

sub nosuid
{
	return shift->{nosuid};
}

sub noexec
{
	return shift->{noexec};
}

sub create
{
	my ($class, $dev, $opts) = @_;
	my $n = bless 
	    { dev => $dev, problems => 0 },
	    $class;
	if (defined $opts) {
		$n->parse_opts($opts);
	}
	return $n;
}

sub new
{
	my ($class, $dev, $opts) = @_;

	if (!defined $devinfo->{$dev}) {
		$devinfo->{$dev} = $class->create($dev, $opts);
	}
	return $devinfo->{$dev};
}

sub run
{
	my $state = shift;
	my $code = pop;
	open(my $cmd, "-|", @_) or
		$state->errsay("Can't run ",join(' ', @_))
		and return;
	my $_;
	while (<$cmd>) {
		&$code($_);
	}
	if (!close($cmd)) {
		if ($!) {
			$state->errsay("Error running ", join(' ', @_),": $!");
		} else {
			$state->errsay("Exit status $? from ", join(' ', @_));
		}
	}
}

sub ask_mount
{
	my $state = shift;

	$giveup = OpenBSD::MountPoint::Fail->new;
	delete $ENV{'BLOCKSIZE'};
	run($state, OpenBSD::Paths->mount, sub {
		my $_ = shift;
		chomp;
		if (m/^(.*?)\s+on\s+\/.*?\s+type\s+.*?(?:\s+\((.*?)\))?$/o) {
			my ($dev, $opts) = ($1, $2);
			OpenBSD::MountPoint->new($dev, $opts);
		} else {
			$state->errsay("Can't parse mount line: $_");
		}
	});
}

sub ask_df
{
	my ($fname, $state) = @_;

	my $info = $giveup;
	my $blocksize = 512;

	run($state, OpenBSD::Paths->df, "--", $fname, sub {
		my $_ = shift;
		chomp;
		if (m/^Filesystem\s+(\d+)\-blocks/o) {
			$blocksize = $1;
		} elsif (m/^(.*?)\s+\d+\s+\d+\s+(\-?\d+)\s+\d+\%\s+\/.*?$/o) {
			my ($dev, $avail) = ($1, $2);
			$info = $devinfo->{$dev};
			if (!defined $info) {
				$info = OpenBSD::MountPoint->new($dev);
			}
			$info->{avail} = $avail;
			$info->{blocksize} = $blocksize;
		}
	});

	return $info;
}

sub find
{
	my ($class, $dev, $fname, $state) = @_;
	ask_mount($state) if !defined $devinfo;
	if (!defined $dev) {
		return $giveup;
	}
	my $info = $devinfo->{$dev};
	if (!defined $info->{avail}) {
		$info = ask_df($fname, $state);
	}
	return $info;
}

sub compute_avail
{
	my ($self, $used) = @_;
	return $self->{avail} - $used/$self->{blocksize};
}

sub avail
{
	my $self = shift;

	return $self->compute_avail($self->{used});
}

sub report_ro
{
	my ($s, $state, $fname) = @_;

	if ($state->verbose >= 3 or ++($s->{problems}) < 4) {
		$state->errsay("Error: ", $s->{dev}, 
		    " is read-only ($fname)");
	} elsif ($s->{problems} == 4) {
		$state->errsay("Error: ... more files on ", $s->{dev});
	}
	$state->{problems}++;
}

sub report_overflow
{
	my ($s, $state, $fname) = @_;

	if ($state->verbose >= 3 or ++($s->{problems}) < 4) {
		$state->errsay("Error: ", $s->{dev}, 
		    " is not large enough ($fname)");
	} elsif ($s->{problems} == 4) {
		$state->errsay("Error: ... more files do not fit on ", 
		    $s->{dev});
	}
	$state->{problems}++;
	$state->{overflow} = 1;
}

sub report_noexec
{
	my ($s, $state, $fname) = @_;
	$state->errsay("Error: ", $s->{dev}, " is noexec ($fname)");
	$state->{problems}++;
}

package OpenBSD::MountPoint::Fail;
our @ISA=qw(OpenBSD::MountPoint);

sub new
{
	my $class = shift;
	bless { avail => 0, dev => '???' }, $class;
}

sub compute_avail
{
	return 1;
}

package OpenBSD::MountPoint::Proxy;

sub new
{
	my ($class, $used, $mp) = @_;
	bless {real => $mp, used => $used, delayed => 0}, $class;
}

sub ro
{
	return shift->{real}->ro;
}

sub noexec
{
	return shift->{real}->noexec;
}

sub avail
{
	my $self = shift;
	return $self->{real}->compute_avail($self->{used});
}

sub name
{
	my $self = shift;
	return $self->{real}->{dev};
}

1;
