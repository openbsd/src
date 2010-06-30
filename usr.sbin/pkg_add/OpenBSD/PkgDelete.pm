#!/usr/bin/perl
# ex:ts=8 sw=4:
# $OpenBSD: PkgDelete.pm,v 1.8 2010/06/30 10:51:04 espie Exp $
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

use OpenBSD::AddDelete;

package OpenBSD::PkgDelete::State;
our @ISA = qw(OpenBSD::AddDelete::State);

sub handle_options
{
	my $state = shift;
	$state->SUPER::handle_options('',
	    '[-cIinqsvx] [-B pkg-destdir] [-D name[=value]] pkg-name [...]');

	my $base = $state->opt('B') // $ENV{'PKG_DESTDIR'} // '';
	if ($base ne '') {
		$base.='/' unless $base =~ m/\/$/o;
	}
	$ENV{'PKG_DESTDIR'} = $base;

	$state->{destdir} = $base;
	if ($base eq '') {
	    $state->{destdirname} = '';
	} else {
	    $state->{destdirname} = '${PKG_DESTDIR}';
	}
}

sub todo
{
	my ($state, $offset) = @_;
	return sprintf("%u/%u", $state->{done} - $offset, $state->{total});
}

package OpenBSD::PkgDelete;
our @ISA = qw(OpenBSD::AddDelete);

use OpenBSD::PackingList;
use OpenBSD::RequiredBy;
use OpenBSD::Delete;
use OpenBSD::PackageInfo;
use OpenBSD::UpdateSet;


my %done;
my $removed;

# Resolve pkg names
my @realnames;
my @todo;

sub process_parameters
{
	my ($self, $state) = @_;
	OpenBSD::PackageInfo::solve_installed_names(\@ARGV, \@realnames,
	    "(removing them all)", $state);

	@todo = OpenBSD::RequiredBy->compute_closure(@realnames);

	if (@todo > @realnames) {
		my $details = $state->verbose >= 2 ||
		    $state->defines('verbosedeps');
		my $show = sub {
			my ($p, $d) = @_;
			$state->say("Can't remove #1".
			    " without also removing:\n#2",
			    join(' ', @$p), join(' ', @$d));
		};
		if ($state->{interactive} || !$details) {
			my %deps = map {($_, 1)} @todo;
			for my $p (@realnames) {
				delete $deps{$p};
			}
			&$show([@realnames], [keys %deps]);
			if (@realnames > 1 && (keys %deps) > 1 &&
			    $state->confirm("Do you want details", 1)) {
				$details = 1;
			}
		}
		if ($details) {
			for my $pkg (@realnames) {
				my @deps = OpenBSD::RequiredBy->compute_closure($pkg);
				next unless @deps > 1;
				@deps = grep {$_ ne $pkg} @deps;
				&$show([$pkg], [@deps]);
			}
		}
		my $them = @todo > 1 ? 'them' : 'it';
		if ($state->defines('dependencies') or
		    $state->confirm("Do you want to remove $them as well", 0)) {
			$state->say("(removing #1 as well)", $them);
		} else {
			$state->{bad}++;
		}
	}
}

sub finish_display
{
}

sub main
{
	my ($self, $state) = @_;

	# and finally, handle the removal
	do {
		$removed = 0;
		if ($state->{not}) {
			$state->status->what("Pretending to delete");
		} else {
			$state->status->what("Deleting");
		}
		$state->{total} = scalar @todo;
		DELETE: for my $pkgname (@todo) {
			if ($done{$pkgname}) {
				next;
			}
			unless (is_installed($pkgname)) {
				$state->errsay("#1 was not installed", $pkgname);
				$done{$pkgname} = 1;
				$removed++;
				next;
			}
			my $r = OpenBSD::RequiredBy->new($pkgname);
			if ($r->list > 0) {
				if ($state->defines('baddepend')) {
					for my $p ($r->list) {
						if ($done{$p}) {
							$r->delete($p);
						} else {
							next DELETE;
						}
					}
				} else {
					next;
				}
			}
			my $info = sub {
			};

			$state->status->object($pkgname);
			if (!$state->progress->set_header($pkgname)) {
				$state->say($state->{not} ?
				    "Pretending to delete #1" :
				    "Deleting #1",
				    $pkgname) if $state->verbose;
			}
			$state->log->set_context('-'.$pkgname);
			OpenBSD::Delete::delete_package($pkgname, $state);
			$done{$pkgname} = 1;
			$removed++;
		}
	} while ($removed);
}

sub new_state
{
	my ($self, $cmd) = @_;
	return OpenBSD::PkgDelete::State->new($cmd);
}

1;
