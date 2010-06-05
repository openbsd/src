#! /usr/bin/perl

# ex:ts=8 sw=4:
# $OpenBSD: PkgFsck.pm,v 1.1 2010/06/05 12:30:36 espie Exp $
#
# Copyright (c) 2003-2010 Marc Espie <espie@openbsd.org>
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

use OpenBSD::AddCreateDelete;

package OpenBSD::PackingElement;
sub thorough_check
{
	my ($self, $state) = @_;
	$self->basic_check($state);
}

sub basic_check
{
}

package OpenBSD::PackingElement::FileBase;
use File::Basename;

sub basic_check
{
	my ($self, $state) = @_;

	my $name = $self->fullname;
	$state->{known}{dirname($name)}{basename($name)} = 1;
	if (!-e $name) {
		$state->log("$name should exist\n");
	}
	if ($self->{symlink}) {
		if (!-l $name) {
			$state->log("$name is not a symlink\n");
		}
		return;
	}
	if (!-f _) {
		$state->log("$name is not a file\n");
	}
}

sub thorough_check
{
	my ($self, $state) = @_;
	$self->basic_check($state);
	return if $self->{link} or $self->{symlink} or $self->{nochecksum};
	if (!-r $self->fullname) {
		$state->log("can't read ", $self->fullname, "\n");
		return;
	}
	my $d = $self->compute_digest($self->fullname);
	if (!$d->equals($self->{d})) {
		$state->log("checksum for ", $self->fullname, " does not match", "\n");
	}
}

package OpenBSD::PackingElement::SpecialFile;
sub basic_check
{
	&OpenBSD::PackingElement::FileBase::basic_check;
}

sub thorough_check
{
	&OpenBSD::PackingElement::FileBase::basic_check;
}

package OpenBSD::PackingElement::DirlikeObject;
sub basic_check
{
	my ($self, $state) = @_;
	my $name = $self->fullname;
	$state->{known}{$name} //= {};
	if (!-e $name) {
		$state->log("$name should exist\n");
	}
	if (!-d _) {
		$state->log("$name is not a directory\n");
	}
}

package OpenBSD::PackingElement::Mandir;
sub basic_check
{
	my ($self, $state) = @_;
	$self->SUPER::basic_check($state);
	$state->{known}{$self->fullname}{'whatis.db'} = 1;
}

package OpenBSD::PackingElement::Fontdir;
sub basic_check
{
	my ($self, $state) = @_;
	$self->SUPER::basic_check($state);
	for my $i (qw(fonts.alias fonts.scale fonts.dir)) {
		$state->{known}{$self->fullname}{$i} = 1;
	}
}

package OpenBSD::PackingElement::Infodir;
sub basic_check
{
	my ($self, $state) = @_;
	$self->SUPER::basic_check($state);
	$state->{known}{$self->fullname}{'dir'} = 1;
}

package OpenBSD::Log;
use OpenBSD::Error;
our @ISA = qw(OpenBSD::Error);

sub set_context
{
	&OpenBSD::Error::set_pkgname;
}

sub dump
{
	&OpenBSD::Error::delayed_output;
}

package OpenBSD::PkgFsck::State;
our @ISA = qw(OpenBSD::AddCreateDelete::State);

sub init
{
	my $self = shift;
	$self->{l} = OpenBSD::Log->new;
	$self->SUPER::init;
}

sub log
{
	my $self = shift;
	if (@_ == 0) {
		return $self->{l};
	} else {
		$self->{l}->print(@_);
	}
}

package OpenBSD::PkgFsck;
our @ISA = qw(OpenBSD::AddCreateDelete);

use OpenBSD::PackageInfo;
use OpenBSD::PackingList;
use File::Find;
use OpenBSD::Paths;
use OpenBSD::Mtree;

sub remove
{
	my ($self, $state, $name) = @_;
	$state->{removed}{$name} = 1;
	my $dir = installed_info($name);
	for my $i (@OpenBSD::PackageInfo::info) {
		if (-e $dir.$i) {
			if ($state->verbose) {
				$state->say("unlink($dir.$i)");
			}
			unless ($state->{not}) {
				unlink($dir.$i) or
				    $state->errsay("$name: Couldn't delete $dir.$i: $!");
			}
		}
	}
	if (-f $dir) {
		if ($state->verbose) {
			$state->say("unlink($dir)");
		}
		unless ($state->{not}) {
			unlink($dir) or
			    $state->errsay("$name: Couldn't delete $dir: $!");
		}
	} elsif (-d $dir) {
		if ($state->verbose) {
			$state->say("rmdir($dir)");
		}
		unless ($state->{not}) {
			rmdir($dir) or
			    $state->errsay("$name: Couldn't delete $dir: $!");
		}
	}
}

sub may_remove
{
	my ($self, $state, $name) = @_;
	if ($state->{force}) {
		$self->remove($state, $name);
	} elsif ($state->{interactive}) {
		require OpenBSD::Interactive;
		if (OpenBSD::Interactive("Remove wrong package $name ?")) {
			$self->remove($state, $name);
		}
	}
}

sub sanity_check
{
	my ($self, $state, $l) = @_;
	$state->progress->set_header("Packing-list sanity");
	my $i = 0;
	for my $name (@$l) {
		my $info = installed_info($name);
		$state->progress->show(++$i, scalar @$l);
		if (-f $info) {
			$state->errsay("$name: $info should be a directory");
			if ($info =~ m/\.core$/) {
				$state->errsay("looks like a core dump, ",
					"removing");
				$self->remove($state, $name);
			} else {
				$self->may_remove($state, $name);
			}
			next;
		}
		my $contents = $info.OpenBSD::PackageInfo::CONTENTS;
		unless (-f $contents) {
			$state->errsay("$name: missing $contents");
			$self->may_remove($state, $name);
			next;
		}
		my $plist;
		eval {
			$plist = OpenBSD::PackingList->fromfile($contents);
		};
		if ($@) {
			$state->errsay("$name: bad plist");
			$self->may_remove($state, $name);
			next;
		}
		if ($plist->pkgname ne $name) {
			$state->errsay("$name: pkgname does not match");
			$self->may_remove($state, $name);
		}
	}
}

sub package_files_check
{
	my ($self, $state, $l) = @_;
	$state->progress->set_header("Files from packages");
	my $i = 0;
	for my $name (@$l) {
		next if $state->{removed}{$name};
		$state->progress->show(++$i, scalar @$l);
		my $plist = OpenBSD::PackingList->from_installation($name);
		$state->log->set_context($name);
		if ($state->{quick}) {
			$plist->basic_check($state);
		} else {
			$plist->thorough_check($state);
		}
	}
}

sub localbase_check
{
	my ($self, $state) = @_;
	$state->{known} //= {};
	# XXX
	OpenBSD::Mtree::parse($state->{known}, OpenBSD::Paths->localbase,
	    "/etc/mtree/BSD.local.dist", 1);
	$state->progress->set_header("Other files");
	find(sub {
		$state->progress->working(1024);
		if (-d $_) {
			return if defined $state->{known}{$File::Find::name};
		} else {
			return if $state->{known}{$File::Find::dir}{$_};
		}
		$state->say("Unknown object $File::Find::name");
	}, OpenBSD::Paths->localbase);
}

sub run
{
	my ($self, $state) = @_;

	my @list = installed_packages(1);
	$self->sanity_check($state, \@list);
	$self->package_files_check($state, \@list);
	$self->localbase_check($state);
}

sub parse_and_run
{
	my $self = shift;

	my $state = OpenBSD::PkgFsck::State->new;
	$self->handle_options('fiq', $state,
		'pkg_fsck [-fimnqvx] [-D value]');
	$state->{interactive} = $state->opt('i');
	$state->{force} = $state->opt('f');
	$state->{quick} = $state->opt('q');
	$self->run($state);
	$state->log->dump;
}

1;
