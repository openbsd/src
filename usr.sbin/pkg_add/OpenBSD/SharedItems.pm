# ex:ts=8 sw=4:
# $OpenBSD: SharedItems.pm,v 1.17 2009/12/17 11:57:02 espie Exp $
#
# Copyright (c) 2004-2006 Marc Espie <espie@openbsd.org>
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

package OpenBSD::SharedItems;

use OpenBSD::Error;
use OpenBSD::PackageInfo;
use OpenBSD::PackingList;
use OpenBSD::Paths;

sub find_items_in_installed_packages
{
	my $progress = shift;
	my $db = OpenBSD::SharedItemsRecorder->new;
	my @list = installed_packages();
	my $total = @list;
	$progress->set_header("Read shared items");
	my $done = 0;
	for my $e (@list) {
		$progress->show($done, $total);
		my $plist = OpenBSD::PackingList->from_installation($e, 
		    \&OpenBSD::PackingList::SharedItemsOnly) or next;
		next if !defined $plist;
		$plist->record_shared($db, $e);
		$done++;
	}
	return $db;
}

sub cleanup
{
	my ($recorder, $state) = @_;

	my $remaining = find_items_in_installed_packages($state->progress);

	$state->progress->clear;
	$state->progress->set_header("Clean shared items");
	my $h = $recorder->{dirs};
	my $u = $recorder->{users};
	my $g = $recorder->{groups};
	my $total = 0;
	$total += keys %$h if defined $h;
	$total += keys %$u if defined $u;
	$total += keys %$g if defined $g;
	my $done = 0;

	if (defined $h) {
		for my $d (sort {$b cmp $a} keys %$h) {
			$state->progress->show($done, $total);
			my $realname = $state->{destdir}.$d;
			if ($remaining->{dirs}->{$realname}) {
				for my $i (@{$h->{$d}}) {
					$state->log->set_context($i->{pkgname});
					$i->reload($state);
				}
			} else {
				for my $i (@{$h->{$d}}) {
					$state->log->set_context($i->{pkgname});
					$i->cleanup($state);
				}
				if (!rmdir $realname) {
					$state->log("Error deleting directory $realname: $!\n")
					    unless $state->{dirs_okay}->{$d};
				}
			}
			$done++;
		}
	}
	if (defined $u) {
		while (my ($user, $pkgname) = each %$u) {
			$state->progress->show($done, $total);
			next if $remaining->{users}->{$user};
			if ($state->{extra}) {
				$state->system(OpenBSD::Paths->userdel, '--', 
				    $user);
			} else {
				$state->log->set_context($pkgname);
				$state->log("You should also run /usr/sbin/userdel $user\n");
			}
			$done++;
		}
	}
	if (defined $g) {
		while (my ($group, $pkgname) = each %$g) {
			$state->progress->show($done, $total);
			next if $remaining->{groups}->{$group};
			if ($state->{extra}) {
				$state->system(OpenBSD::Paths->groupdel, '--',
				    $group);
			} else {
				$state->log->set_context($pkgname);
				$state->log("You should also run /usr/sbin/groupdel $group\n");
			}
			$done++;
		}
	}
	$state->progress->next;
}

package OpenBSD::PackingElement;
sub cleanup
{
}

sub reload
{
}

package OpenBSD::PackingElement::Mandir;
sub cleanup
{
	my ($self, $state) = @_;
	my $fullname = $state->{destdir}.$self->fullname;
	$state->log("You may wish to remove ", $fullname, " from man.conf\n");
	for my $f (OpenBSD::Paths->man_cruft) {
		unlink("$fullname/$f");
	}
}

package OpenBSD::PackingElement::Fontdir;
sub cleanup
{
	my ($self, $state) = @_;
	my $fullname = $state->{destdir}.$self->fullname;
	$state->log("You may wish to remove ", $fullname, " from your font path\n");
	for my $f (OpenBSD::Paths->font_cruft) {
		unlink("$fullname/$f");
	}
}

package OpenBSD::PackingElement::Infodir;
sub cleanup
{
	my ($self, $state) = @_;
	my $fullname = $state->{destdir}.$self->fullname;
	for my $f (OpenBSD::Paths->info_cruft) {
		unlink("$fullname/$f");
	}
}

1;
