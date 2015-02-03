#! /usr/bin/perl

# ex:ts=8 sw=4:
# $OpenBSD: FwUpdate.pm,v 1.13 2015/02/03 09:58:15 espie Exp $
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
use OpenBSD::PackageRepository;

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
		if ($tag eq '-current') {
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
	$state->progress->setup(0, 0, $state);
	$state->{localbase} = OpenBSD::Paths->localbase;
	$state->{destdir} = '';
	$state->{wantntogo} = 0;
	$state->{interactive} = OpenBSD::InteractiveStub->new($state);
	$state->{subst}->add('repair', 1);
	if ($state->opt('a') && @ARGV != 0) {
		$state->usage;
	}
	$state->{fw_repository} = 
	    OpenBSD::PackageRepository->new($state->{path}, $state);
	$state->{fw_verbose} = $state->{v};
	if ($state->{v}) {
		$state->{v}--;
	}
	if ($state->{fw_verbose}) {
		$state->say("Path to firmware: #1", $state->{path});
	}
}

sub finish_init
{
	my $state = shift;
	delete $state->{signer_list}; # XXX uncache value
	$state->{subst}->add('FW_UPDATE', 1);
}

sub installed_drivers
{
	my $self = shift;
	return keys %{$self->{installed_drivers}};
}

sub is_installed
{
	my ($self, $driver) = @_;
	return $self->{installed_drivers}{$driver};
}

sub machine_drivers
{
	my $self = shift;
	return keys %{$self->{machine_drivers}};
}

sub all_drivers
{
	my $self = shift;
	return keys %{$self->{all_drivers}};
}

sub is_needed
{
	my ($self, $driver) = @_;
	return $self->{machine_drivers}{$driver};
}

sub display_timestamp
{
	my ($state, $pkgname, $timestamp) = @_;
	return unless $state->verbose;
	$state->SUPER::display_timestamp($pkgname, $timestamp);
}

sub fw_status
{
	my ($state, $msg, $list) = @_;
	return if @$list == 0;
	$state->say("#1: #2", $msg, join(' ', @$list));
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
	my ($self, $state) = @_;
	$state->{machine_drivers} = {};
	$state->{all_drivers} = \%possible_drivers;
	my %search = %possible_drivers;
	if (open(my $f, '<', '/var/run/dmesg.boot')) {
		$self->parse_dmesg($f, \%search, $state->{machine_drivers});
		close($f);
	} else {
		$state->errsay("Can't open dmesg.boot: #1", $!);
	}
	if (open(my $cmd, '-|', 'dmesg')) {
		$self->parse_dmesg($cmd, \%search, $state->{machine_drivers});
		close($cmd);
	} else {
		$state->errsay("Can't run dmesg: #1", $!);
	}
}

sub find_installed_drivers
{
	my ($self, $state) = @_;
	my $inst = $state->repo->installed;
	for my $driver (keys %possible_drivers) {	
		my $search = OpenBSD::Search::Stem->new("$driver-firmware");
		my $l = $inst->match_locations($search);
		if (@$l > 0) {
			$state->{installed_drivers}{$driver} = 
			    OpenBSD::Handle->from_location($l->[0]);
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
	my ($self, $state, $driver) = @_;
	my $pkgname = "$driver-firmware";
	my $set;
	my $h = $state->is_installed($driver);
	if ($h) {
		$set = $state->updateset->add_older($h);
	} else {
		$set = $state->updateset->add_hints($pkgname);
	}
	$set->add_repositories($state->{fw_repository});
	return $set;
}

sub mark_set_for_deletion
{
	my ($self, $set, $state) = @_;
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
	$self->SUPER::do_quirks($state);
	$state->finish_init;
}

sub to_remove
{
	my ($self, $state, $driver) = @_;
	$self->mark_set_for_deletion($self->to_add_or_update($state, $driver));
}

sub to_add_or_update
{
	my ($self, $state, $driver) = @_;
	my $set = $self->find_handle($state, $driver);
	push(@{$state->{setlist}}, $set);
	return $set;
}

sub process_parameters
{
	my ($self, $state) = @_;

	$self->find_machine_drivers($state);
	$self->find_installed_drivers($state);

	if (@ARGV == 0) {
		if ($state->opt('d')) {
			for my $driver ($state->installed_drivers) {
				if ($state->opt('a') || 
				    !$state->is_needed($driver)) {
					$self->to_remove($state, $driver);
				}
			}
		} else {
			if ($state->opt('a')) {
				for my $driver ($state->all_drivers) {
					$self->to_add_or_update($state, 
					    $driver);
				}
			} else {
				for my $driver ($state->machine_drivers) {
					$self->to_add_or_update($state, 
					    $driver);
				}
				for my $driver ($state->installed_drivers) {
					# XXX skip already done up there ^
					next if $state->is_needed($driver);
					$self->to_add_or_update($state, 
					    $driver);
				}
			}
		}
		if (!(defined $state->{setlist}) && $state->{fw_verbose}) {
			$state->say($state->opt('d') ?
			    "No firmware to delete." :
			    "No devices found which need firmware files to be downloaded.");
			exit(0);
		}
	} else {
		for my $driver (@ARGV) {
			if (!$possible_drivers{$driver}) {
				$state->errsay("#1: unknown driver", $driver);
				exit(1);
			}
			my $set = $self->to_add_or_update($state, $driver);
			if ($state->opt('d')) {
				if (!$state->is_installed($driver)) {
					$state->errsay("Can't delete uninstalled driver: #1", $driver);
					next;
				}
				$self->mark_set_for_deletion($set);
			} 
		}
	}
	if ($state->{fw_verbose}) {
		my (@deleting, @updating, @installing);
		for my $set (@{$state->{setlist}}) {
			for my $h ($set->older) {
				if ($h->{update_found}) {
					push(@deleting, $h->pkgname);
				} else {
					push(@updating, $h->pkgname);
				}
			}
			for my $h ($set->hints) {
				push(@installing, $h->pkgname);
			}
		}
		$state->fw_status("Installing", \@installing);
		$state->fw_status("Deleting", \@deleting);
		$state->fw_status("Updating", \@updating);
	}
}

1;
