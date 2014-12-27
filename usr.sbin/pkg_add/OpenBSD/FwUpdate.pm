#! /usr/bin/perl

# ex:ts=8 sw=4:
# $OpenBSD: FwUpdate.pm,v 1.2 2014/12/27 23:58:52 espie Exp $
#
# Copyright (c) 2014 Marc Espie <espie@openbsd.org>
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
#
use strict;
use warnings;
use OpenBSD::PkgAdd;

package OpenBSD::FwUpdate::State;
our @ISA = qw(OpenBSD::PkgAdd::State);

sub find_path
{
	my $state = shift;
	open my $cmd, '-|', OpenBSD::Paths->sysctl, '-n', 'kern.version';
	my $line = <$cmd>;
	close($cmd);
	if ($line =~ m/^OpenBSD (\d\.\d)(\S*)\s/) {
		my ($version, $tag) = ($1, $2);
		if ($tag ne '-stable') {
			$version = 'snapshots';
		}
		$state->{path} = "http://firmware.openbsd.org/firmware/$version/";
	} else {
		$state->fatal("Couldn't find/parse OS version");
	}
}

sub handle_options
{
	my $state = shift;
	$state->OpenBSD::State::handle_options('adnp:', 
	    '[-adnv] [-D keyword] [-p path] [driver...]');
	$state->{not} = $state->opt('n');
	if ($state->opt('p')) {
		$state->{path} = $state->opt('p');
	} else {
		$state->find_path;
	}
	$main::not = $state->{not};
	$state->{localbase} = OpenBSD::Paths->localbase;
	$state->{destdir} = '';
	$state->{wantntogo} = 0;
	$state->{subst}->add('repair', 1);
	$state->finish_init;
}

sub finish_init
{
	my $state = shift;
	$ENV{PKG_PATH} = $state->{path};
	if ($state->verbose && !$state->opt('d')) {
		$state->say("PKG_PATH=#1", $state->{path});
	}
	$state->{subst}->add('FW_UPDATE', 1);
}

package OpenBSD::FwUpdate::Update;
our @ISA = qw(OpenBSD::Update);

package OpenBSD::FwUpdate;
our @ISA = qw(OpenBSD::PkgAdd);

OpenBSD::Auto::cache(updater,
    sub {
	    require OpenBSD::Update;
	    return OpenBSD::FwUpdate::Update->new;
    });

my %possible_drivers =  map {($_, 1)}
    (qw(acx athn bwi ipw iwi iwn malo otus pgt radeondrm rsu uath
	upgt urtwn uvideo wpi));


sub parse_dmesg
{
	my ($self, $f, $search, $found) = @_;

	while (<$f>) {
		chomp;
		for my $driver (keys %$search) {
			next unless m/^\Q$driver\E\d+\s+at\s/;
			delete $search->{$driver};
			$found->{$driver} = 1;
		}
	}
}

sub find_machine_drivers
{
	my ($self, $state, $h) = @_;
	my %search = %possible_drivers;
	if (open(my $f, '<', '/var/run/dmesg.boot')) {
		$self->parse_dmesg($f, \%search, $h);
		close($f);
	} else {
		$state->errsay("Can't open dmesg.boot: #1", $!);
	}
	if (open(my $cmd, '-|', 'dmesg')) {
		$self->parse_dmesg($cmd, \%search, $h);
		close($cmd);
	} else {
		$state->errsay("Can't run dmesg: #1", $!);
	}
}

sub find_installed_drivers
{
	my ($self, $state, $h) = @_;
	require OpenBSD::PackageInfo;
	my $list = OpenBSD::PackageInfo->installed_stems;
	for my $driver (keys %possible_drivers) {	
		if ($list->find("$driver-firmware")) {
			$h->{$driver} = 1;
		}
	}
}


sub new_state
{
	my ($self, $cmd) = @_;
	return OpenBSD::FwUpdate::State->new($cmd);
}

sub find_handle
{
	my ($self, $state, $done, $inst, $driver) = @_;
	my $pkgname = "$driver-firmware";
	if ($done->{$driver}) {
		my $l = $state->updater->stem2location($inst, $pkgname, $state);
		return $state->updateset->add_older(OpenBSD::Handle->from_location($l));
	} else {
		return $state->updateset->add_hints($pkgname);
	}
}

sub mark_set_for_deletion
{
	my ($self, $set) = @_;
	# XXX to be simplified. Basically, we pre-do the work of the updater...
	for my $h ($set->older) {
		$h->{update_found} = 1;
	}
	$set->{updates}++;
}

# no way we can find a new quirks on the firmware site
sub do_quirks
{
	my ($self, $state) = @_;
#	$self->SUPER::do_quirks($state);
#	$state->finish_init;
}

sub process_parameters
{
	my ($self, $state) = @_;

	my $todo = {};
	my $done = {};
	$self->find_machine_drivers($state, $todo);
	$self->find_installed_drivers($state, $done);
	my $inst = $state->repo->installed;

	if (@ARGV == 0) {
		for my $driver (keys %$todo) {
			push(@{$state->{setlist}}, 
			    $self->find_handle($state, $done, $inst, $driver));
		}
	} else {
		for my $driver (@ARGV) {
			my $set = $self->find_handle($state, $done, $inst, 
			    $driver);
			if ($state->opt('d')) {
				if (!$done->{$driver}) {
					$state->errsay("Can't delete uninstalled driver: #1", $driver);
					next;
				}
				$self->mark_set_for_deletion($set);
			} 
			push(@{$state->{setlist}}, $set);
		}
	}
}

1;
