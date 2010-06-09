#! /usr/bin/perl

# ex:ts=8 sw=4:
# $OpenBSD: PkgCheck.pm,v 1.10 2010/06/09 07:26:01 espie Exp $
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
use OpenBSD::SharedLibs;

package OpenBSD::PackingElement;
sub thorough_check
{
	my ($self, $state) = @_;
	$self->basic_check($state);
}

sub basic_check
{
}

sub find_dependencies
{
}

package OpenBSD::PackingElement::FileBase;
use File::Basename;

sub basic_check
{
	my ($self, $state) = @_;

	my $name = $state->{destdir}.$self->fullname;
	$state->{known}{dirname($name)}{basename($name)} = 1;
	if ($self->{symlink}) {
		if (!-l $name) {
			if (!-e $name) {
				$state->log("#1 should be a symlink but does not exist", $name);
			} else {
				$state->log("#1 is not a symlink", $name);
			}
		} else {
			if (readlink($name) ne $self->{symlink}) {
				$state->log("#1 should point to #2 but points to #3 instead", 
				    $name, $self->{symlink}, readlink($name));
			}
		}
		return;
	}
	if (!-e $name) {
		if (-l $name) {
			$state->log("#1 points to non-existent #2", 
			    $name, readlink($name));
		} else {
			$state->log("#1 should exist", $name);
		}
	}
	if (!-f _) {
		$state->log("#1 is not a file", $name);
	}
	if ($self->{link}) {
		my ($a, $b) = (stat _)[0, 1];
		if (!-f $state->{destdir}.$self->{link}) {
			$state->log("#1 should link to non-existent #2", 
			    $name, $self->{link});
		} else {
			my ($c, $d) = (stat _)[0, 1];
			if (defined $a && defined $c) {
				if ($a != $c || $b != $d) {
					$state->log("#1 doesn't link to #2",
					    $name, $self->{link});
				}
			}
		}
	}
}

sub thorough_check
{
	my ($self, $state) = @_;
	my $name = $state->{destdir}.$self->fullname;
	$self->basic_check($state);
	return if $self->{link} or $self->{symlink} or $self->{nochecksum};
	if (!-r $name) {
		$state->log("can't read #1", $name);
		return;
	}
	my $d = $self->compute_digest($name);
	if (!$d->equals($self->{d})) {
		$state->log("checksum for #1 does not match", $name);
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
	my $name = $state->{destdir}.$self->fullname;
	$state->{known}{$name} //= {};
	if (!-e $name) {
		$state->log("#1 should exist", $name);
	}
	if (!-d _) {
		$state->log("#1 is not a directory", $name);
	}
}

package OpenBSD::PackingElement::Mandir;
sub basic_check
{
	my ($self, $state) = @_;
	$self->SUPER::basic_check($state);
	my $name = $state->{destdir}.$self->fullname;
	$state->{known}{$name}{'whatis.db'} = 1;
}

package OpenBSD::PackingElement::Fontdir;
sub basic_check
{
	my ($self, $state) = @_;
	$self->SUPER::basic_check($state);
	my $name = $state->{destdir}.$self->fullname;
	for my $i (qw(fonts.alias fonts.scale fonts.dir)) {
		$state->{known}{$name}{$i} = 1;
	}
}

package OpenBSD::PackingElement::Infodir;
sub basic_check
{
	my ($self, $state) = @_;
	$self->SUPER::basic_check($state);
	my $name = $state->{destdir}.$self->fullname;
	$state->{known}{$name}{'dir'} = 1;
}

package OpenBSD::PackingElement::Dependency;
sub find_dependencies
{
	my ($self, $state, $l, $checker) = @_;
	# several ways to failure
	if (!$self->spec->is_valid) {
		$state->log("invalid \@", $self->keyword, " ", 
		    $self->stringize);
		return;
	}
	my @deps = $self->spec->filter(@$l);
	if (@deps == 0) {
		$state->log("dependency #1 does not match any installed package",
		    $self->stringize);
		return;
	}
	my $okay = 0;
	for my $i (@deps) {
		if ($checker->find($i)) {
			$okay = 1;
		}
	}
	if (!$okay) {
		$checker->not_found($deps[0]);
	}
}

package OpenBSD::PackingElement::Wantlib;
sub find_dependencies
{
	my ($self, $state, $l, $checker) = @_;
	my $r = OpenBSD::SharedLibs::lookup_libspec($state->{localbase}, 
	    $self->spec);
	if (defined $r && @$r != 0) {
		my $okay = 0;
		for my $lib (@$r) {
			my $i = $lib->origin;
			if ($i eq 'system') {
				$okay = 1;
				next;
			}
			if ($checker->find($i)) {
				$okay = 1;
			}
		}
		if (!$okay) {
			$checker->not_found($r->[0]->origin);
		}
	} else {
		$state->log("#1 not found", $self->stringize);
	}
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

package OpenBSD::PkgCheck::State;
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

package OpenBSD::DependencyCheck;

sub new
{
	my ($class, $state, $name, $req) = @_;
	my $o = bless {
		not_yet => {}, 
		possible => {}, 
		others => {}, 
		name => $name,
		req => $req
	    }, $class;
	for my $pkg ($req->list) {
		$o->{not_yet}{$pkg} = 1;
		if ($state->{exists}{$pkg}) {
			$o->{possible}{$pkg} = 1;
		} else {
			$state->errsay("#1: bogus #2", $name, $o->string($pkg));
		}
	}
	return $o;
}

sub find
{
	my ($self, $name) = @_;
	if ($self->{possible}{$name}) {
		delete $self->{not_yet}{$name};
		return 1;
	} else {
		return 0;
	}
}

sub not_found
{
	my ($self, $name) = @_;
	$self->{others}{$name} = 1;
}

sub ask_delete_deps
{
	my ($self, $state, $l) = @_;
	if ($state->{force}) {
		$self->{req}->delete(@$l);
	} elsif ($state->{interactive}) {
		require OpenBSD::Interactive;
		if (OpenBSD::Interactive::confirm("Remove missing ".
		    $self->string(@$l))) {
			$self->{req}->delete(@$l);
		}
	}
}

sub ask_add_deps
{
	my ($self, $state, $l) = @_;
	if ($state->{force}) {
		$self->{req}->add(@$l);
	} elsif ($state->{interactive}) {
		require OpenBSD::Interactive;
		if (OpenBSD::Interactive::confirm("Add missing ".
		    $self->string(@$l))) {
			$self->{req}->add(@$l);
		}
	}
}

sub adjust
{
	my ($self, $state) = @_;
	if (keys %{$self->{not_yet}} > 0) {
		my @todo = sort keys %{$self->{not_yet}};
		unless ($state->{subst}->value("weed_libs")) {
			@todo = grep {!/^\.libs/} @todo;
		}
		if (@todo != 0) {
			$state->errsay("#1 is having too many #2",
			    $self->{name}, $self->string(@todo));
			$self->ask_delete_deps($state, \@todo);
		}
	}
	if (keys %{$self->{others}} > 0) {
		my @todo = sort keys %{$self->{others}};
		$state->errsay("#1 is missing #2",
		    $self->{name}, $self->string(@todo));
		$self->ask_add_deps($state, \@todo);
	}
}

package OpenBSD::DirectDependencyCheck;
our @ISA = qw(OpenBSD::DependencyCheck);
use OpenBSD::RequiredBy;
sub string 
{ 
	my $self = shift;
	if (@_ == 1) {
		return "dependency: @_";
	} else {
		return "dependencies: ". join(' ', @_);
	}
}

sub new
{
	my ($class, $state, $name) = @_;
	return $class->SUPER::new($state, $name, 
	    OpenBSD::Requiring->new($name));
}

package OpenBSD::ReverseDependencyCheck;
our @ISA = qw(OpenBSD::DependencyCheck);
use OpenBSD::RequiredBy;
sub string 
{ 
	my $self = shift;
	if (@_ == 1) {
		return "reverse dependency: @_";
	} else {
		return "reverse dependencies: ". join(' ', @_);
	}
}

sub new
{
	my ($class, $state, $name) = @_;
	return $class->SUPER::new($state, $name, 
	    OpenBSD::RequiredBy->new($name));
}

package OpenBSD::PkgCheck;
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
				$state->say("unlink(#1)", $dir.$i);
			}
			unless ($state->{not}) {
				unlink($dir.$i) or
				    $state->errsay("#1: Couldn't delete #2: #3",
				    	$name, $dir.$i, $!);
			}
		}
	}
	if (-f $dir) {
		if ($state->verbose) {
			$state->say("unlink(#1)", $dir);
		}
		unless ($state->{not}) {
			unlink($dir) or
			    $state->errsay("#1: Couldn't delete #2: #3", 
				$name, $dir, $!);
		}
	} elsif (-d $dir) {
		if ($state->verbose) {
			$state->say("rmdir(#1)", $dir);
		}
		unless ($state->{not}) {
			rmdir($dir) or
			    $state->errsay("#1: Couldn't delete #2: #3",
			    	$name, $dir, $!);
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
		if (OpenBSD::Interactive::confirm("Remove wrong package $name")) {
			$self->remove($state, $name);
		}
	}
}

sub for_all_packages
{
	my ($self, $state, $l, $msg, $code) = @_;

	my $total = scalar @$l;
	$state->progress->set_header($msg);
	my $i = 0;
	for my $name (@$l) {
		$state->progress->show(++$i, $total);
		next if $state->{removed}{$name};
		&$code($name);
	}
}

sub sanity_check
{
	my ($self, $state, $l) = @_;
	$self->for_all_packages($state, $l, "Packing-list sanity", sub {
		my $name = shift;
		my $info = installed_info($name);
		if (-f $info) {
			$state->errsay("#1: #2 should be a directory", $name, $info);
			if ($info =~ m/\.core$/) {
				$state->errsay("looks like a core dump, ".
					"removing");
				$self->remove($state, $name);
			} else {
				$self->may_remove($state, $name);
			}
			next;
		}
		my $contents = $info.OpenBSD::PackageInfo::CONTENTS;
		unless (-f $contents) {
			$state->errsay("#1: missing #2", $name, $contents);
			$self->may_remove($state, $name);
			next;
		}
		my $plist;
		eval {
			$plist = OpenBSD::PackingList->fromfile($contents);
		};
		if ($@ || !defined $plist) {
			$state->errsay("#1: bad plist", $name);
			$self->may_remove($state, $name);
			next;
		}
		if ($plist->pkgname ne $name) {
			$state->errsay("#1: pkgname does not match", $name);
			$self->may_remove($state, $name);
		}
		$plist->mark_available_lib($plist->pkgname);
		$state->{exists}{$plist->pkgname} = 1;
	});
}

sub dependencies_check
{
	my ($self, $state, $l) = @_;
	OpenBSD::SharedLibs::add_libs_from_system($state->{destdir});
	$self->for_all_packages($state, $l, "Direct dependencies", sub {
		my $name = shift;
		my $plist = OpenBSD::PackingList->from_installation($name,
		    \&OpenBSD::PackingList::DependOnly);
		my $checker = OpenBSD::DirectDependencyCheck->new($state, 
		    $name);
		$state->{localbase} = $plist->localbase;
		$plist->find_dependencies($state, $l, $checker);
		$checker->adjust($state);
		for my $dep ($checker->{req}->list) {
			push(@{$state->{reverse}{$dep}}, $name);
		}
	});
}

sub reverse_dependencies_check
{
	my ($self, $state, $l) = @_;
	$self->for_all_packages($state, $l, "Reverse dependencies", sub {
		my $name = shift;
		my $checker = OpenBSD::ReverseDependencyCheck->new($state, 
		    $name);
		for my $i (@{$state->{reverse}{$name}}) {
			$checker->find($i) or $checker->not_found($i);
		}
		$checker->adjust($state);
	});
}

sub package_files_check
{
	my ($self, $state, $l) = @_;
	$self->for_all_packages($state, $l, "Files from packages", sub {
		my $name = shift;
		my $plist = OpenBSD::PackingList->from_installation($name);
		$state->log->set_context($name);
		if ($state->{quick}) {
			$plist->basic_check($state);
		} else {
			$plist->thorough_check($state);
		}
		$plist->mark_available_lib($plist->pkgname);
	});
}

sub localbase_check
{
	my ($self, $state) = @_;
	$state->{known} //= {};
	my $base = $state->{destdir}.OpenBSD::Paths->localbase;
	$state->{known}{$base."/man"}{'whatis.db'} = 1;
	$state->{known}{$base."/info"}{'dir'} = 1;
	$state->{known}{$base."/lib/X11"}{'app-defaults'} = 1;
	$state->{known}{$base."/libdata"} = {};
	$state->{known}{$base."/libdata/perl5"} = {};
	# XXX
	OpenBSD::Mtree::parse($state->{known}, $base,
	    "/etc/mtree/BSD.local.dist", 1);
	$state->progress->set_header("Other files");
	find(sub {
		$state->progress->working(1024);
		if (-d $_) {
			return if defined $state->{known}{$File::Find::name};
			if (-l $_) {
				return if $state->{known}{$File::Find::dir}{$_};
			}
			$state->say("Unknown directory #1", $File::Find::name);
		} else {
			return if $state->{known}{$File::Find::dir}{$_};
			$state->say("Unknown file #1", $File::Find::name);
		}
	}, OpenBSD::Paths->localbase);
}

sub run
{
	my ($self, $state) = @_;

	my @list = installed_packages();
	$self->sanity_check($state, \@list);
	$self->dependencies_check($state, \@list);
	$state->log->dump;
	$self->reverse_dependencies_check($state, \@list);
	$state->log->dump;
	$self->package_files_check($state, \@list);
	$state->log->dump;
	$self->localbase_check($state);
	$state->progress->next;
}

sub parse_and_run
{
	my ($self, $cmd) = @_;

	my $state = OpenBSD::PkgCheck::State->new($cmd);
	$self->handle_options('fiq', $state,
		'[-fimnqvx] [-B pkg-destdir] [-D value]');
	if (@ARGV != 0) {
		$state->usage;
	}
	$state->{interactive} = $state->opt('i');
	$state->{force} = $state->opt('f');
	$state->{quick} = $state->opt('q');
	if (defined $state->opt('B')) {
		$state->{destdir} = $state->opt('B');
	} elsif (defined $ENV{'PKG_PREFIX'}) {
		$state->{destdir} = $ENV{'PKG_PREFIX'};
	}
	if (defined $state->{destdir}) {
		$state->{destdir} .= '/';
		$ENV{'PKG_DESTDIR'} = $state->{destdir};
	} else {
		$state->{destdir} = '';
		delete $ENV{'PKG_DESTDIR'};
	}
	lock_db(0) unless $state->{subst}->value('nolock');
	$self->run($state);
}

1;
