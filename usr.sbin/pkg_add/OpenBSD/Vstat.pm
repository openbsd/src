# ex:ts=8 sw=4:
# $OpenBSD: Vstat.pm,v 1.58 2010/05/10 09:17:55 espie Exp $
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

{
package OpenBSD::Vstat::Object;
my $cache = {};
my $x = undef;
my $dummy = bless \$x, __PACKAGE__;

sub new
{
	my ($class, $value) = @_;
	if (!defined $value) {
		return $dummy;
	}
	if (!defined $cache->{$value}) {
		$cache->{$value} = bless \$value, $class;
	}
	return $cache->{$value};
}

sub exists
{
	return 1;
}

sub value
{
	my $self = shift;
	return $$self;
}

sub none
{
	return OpenBSD::Vstat::Object::None->new;
}

}
{
package OpenBSD::Vstat::Object::None;
our @ISA = qw(OpenBSD::Vstat::Object);

my $x = undef;
my $none = bless \$x, __PACKAGE__;

sub exists
{
	return 0;
}

sub new
{
	return $none;
}
}

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
	return OpenBSD::Mounts->find($dev, $fname, $self->{state});
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

	bless {v => [{}], state => $state}, $class;
}

sub exists
{
	my ($self, $name) = @_;
	for my $v (@{$self->{v}}) {
		if (defined $v->{$name}) {
			return $v->{$name}->exists;
		}
	}
	return -e $name;
}

sub value
{
	my ($self, $name) = @_;
	for my $v (@{$self->{v}}) {
		if (defined $v->{$name}) {
			return $v->{$name}->value;
		}
	}
	return undef;
}

sub synchronize
{
	my $self = shift;

	OpenBSD::Mounts->synchronize;
	if ($self->{state}->{not}) {
		# this is the actual stacking case: in pretend mode,
		# I have to put a second vfs on top
		if (@{$self->{v}} == 2) {
			my $top = shift @{$self->{v}};
			while (my ($k, $v) = each %$top) {
				$self->{v}[0]{$k} = $v;
			}
		}
		unshift(@{$self->{v}}, {});
	} else {
		$self->{v} = [{}];
	}
}

sub drop_changes
{
	my $self = shift;

	OpenBSD::Mounts->drop_changes;
	# drop the top layer
	$self->{v}[0] = {};
}

sub add
{
	my ($self, $name, $size, $value) = @_;
	$self->{v}[0]->{$name} = OpenBSD::Vstat::Object->new($value);
	return defined($size) ? $self->account_for($name, $size) : undef;
}

sub remove
{
	my ($self, $name, $size) = @_;
	$self->{v}[0]->{$name} = OpenBSD::Vstat::Object->none;
	return defined($size) ? $self->account_later($name, -$size) : undef;
}

sub remove_first
{
	my ($self, $name, $size) = @_;
	$self->{v}[0]->{$name} = OpenBSD::Vstat::Object->none;
	return defined($size) ? $self->account_for($name, -$size) : undef;
}

sub tally
{
	my $self = shift;

	OpenBSD::Mounts->tally($self->{state});
}

package OpenBSD::Mounts;

my $devinfo;
my $devinfo2;
my $giveup;

sub giveup
{
	if (!defined $giveup) {
		$giveup = OpenBSD::MountPoint::Fail->new;
	}
	return $giveup;
}

sub new
{
	my ($class, $dev, $opts) = @_;

	if (!defined $devinfo->{$dev}) {
		$devinfo->{$dev} = OpenBSD::MountPoint->new($dev, $opts);
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
	my ($class, $state) = shift;

	delete $ENV{'BLOCKSIZE'};
	run($state, OpenBSD::Paths->mount, sub {
		my $_ = shift;
		chomp;
		if (m/^(.*?)\s+on\s+\/.*?\s+type\s+.*?(?:\s+\((.*?)\))?$/o) {
			my ($dev, $opts) = ($1, $2);
			$class->new($dev, $opts);
		} else {
			$state->errsay("Can't parse mount line: $_");
		}
	});
}

sub ask_df
{
	my ($class, $fname, $state) = @_;

	my $info = $class->giveup;
	my $blocksize = 512;

	$class->ask_mount($state) if !defined $devinfo;
	run($state, OpenBSD::Paths->df, "--", $fname, sub {
		my $_ = shift;
		chomp;
		if (m/^Filesystem\s+(\d+)\-blocks/o) {
			$blocksize = $1;
		} elsif (m/^(.*?)\s+\d+\s+\d+\s+(\-?\d+)\s+\d+\%\s+\/.*?$/o) {
			my ($dev, $avail) = ($1, $2);
			$info = $devinfo->{$dev};
			if (!defined $info) {
				$info = $class->new($dev);
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
	if (!defined $dev) {
		return $class->giveup;
	}
	if (!defined $devinfo2->{$dev}) {
		$devinfo2->{$dev} = $class->ask_df($fname, $state);
	}
	return $devinfo2->{$dev};
}

sub synchronize
{
	for my $v (values %$devinfo2) {
		$v->synchronize;
	}
}

sub drop_changes
{
	for my $v (values %$devinfo2) {
		$v->drop_changes;
	}
}

sub tally
{
	my ($self, $state) = @_;

	for my $v ((sort {$a->name cmp $b->name } values %$devinfo2), $self->giveup) {
		$v->tally($state);
	}
}

package OpenBSD::MountPoint;

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

sub new
{
	my ($class, $dev, $opts) = @_;
	my $n = bless { commited_use => 0, used => 0, delayed => 0,
	    hw => 0, dev => $dev }, $class;
	if (defined $opts) {
		$n->parse_opts($opts);
	}
	return $n;
}


sub avail
{
	my ($self, $used) = @_;
	return $self->{avail} - $self->{used}/$self->{blocksize};
}

sub name
{
	my $self = shift;
	return $self->{dev};
}

sub report_ro
{
	my ($s, $state, $fname) = @_;

	if ($state->verbose >= 3 or ++($s->{problems}) < 4) {
		$state->errsay("Error: ", $s->name,
		    " is read-only ($fname)");
	} elsif ($s->{problems} == 4) {
		$state->errsay("Error: ... more files on ", $s->name);
	}
	$state->{problems}++;
}

sub report_overflow
{
	my ($s, $state, $fname) = @_;

	if ($state->verbose >= 3 or ++($s->{problems}) < 4) {
		$state->errsay("Error: ", $s->name,
		    " is not large enough ($fname)");
	} elsif ($s->{problems} == 4) {
		$state->errsay("Error: ... more files do not fit on ",
		    $s->name);
	}
	$state->{problems}++;
	$state->{overflow} = 1;
}

sub report_noexec
{
	my ($s, $state, $fname) = @_;
	$state->errsay("Error: ", $s->name, " is noexec ($fname)");
	$state->{problems}++;
}

sub synchronize
{
	my $v = shift;

	if ($v->{used} > $v->{hw}) {
		$v->{hw} = $v->{used};
	}
	$v->{used} += $v->{delayed};
	$v->{delayed} = 0;
	$v->{commited_use} = $v->{used};
}

sub drop_changes
{
	my $v = shift;

	$v->{used} = $v->{commited_use};
	$v->{delayed} = 0;
}

sub tally
{
	my ($data, $state) = @_;

	return  if $data->{used} == 0;
	$state->print($data->name, ": ", $data->{used}, " bytes");
	my $avail = $data->avail;
	if ($avail < 0) {
		$state->print(" (missing ", int(-$avail+1), " blocks)");
	} elsif ($data->{hw} >0 && $data->{hw} > $data->{used}) {
		$state->print(" (highwater ", $data->{hw}, " bytes)");
	}
	$state->say;
}

package OpenBSD::MountPoint::Fail;
our @ISA=qw(OpenBSD::MountPoint);

sub avail
{
	return 1;
}

sub new
{
	my $class = shift;
	my $n = $class->SUPER::new('???');
	$n->{avail} = 0;
	return $n;
}

1;
