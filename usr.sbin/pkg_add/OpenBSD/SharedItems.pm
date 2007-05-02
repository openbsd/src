# ex:ts=8 sw=4:
# $OpenBSD: SharedItems.pm,v 1.9 2007/05/02 15:05:30 espie Exp $
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

use OpenBSD::ProgressMeter;
use OpenBSD::Error;
use OpenBSD::PackageInfo;
use OpenBSD::PackingList;

sub find_items_in_installed_packages
{
	my $db = {dirs=>{}, users=>{}, groups=>{}};
	my @list = installed_packages();
	my $total = @list;
	OpenBSD::ProgressMeter::set_header("Read shared items");
	my $done = 0;
	for my $e (@list) {
		OpenBSD::ProgressMeter::show($done, $total);
		my $plist = OpenBSD::PackingList->from_installation($e, 
		    \&OpenBSD::PackingList::SharedItemsOnly) or next;
		$plist->record_shared_item($e, $db);
		$done++;
	}
	return $db;
}

sub cleanup
{
	my $state = shift;

	my $h = $state->{dirs_to_rm};
	my $u = $state->{users_to_rm};
	my $g = $state->{groups_to_rm};
	return unless defined $h or defined $u or defined $g;
	my $remaining = find_items_in_installed_packages();

	OpenBSD::ProgressMeter::clear();
	OpenBSD::ProgressMeter::set_header("Clean shared items");
	my $total = 0;
	$total += keys %$h if defined $h;
	$total += keys %$u if defined $u;
	$total += keys %$g if defined $g;
	my $done = 0;

	if (defined $h) {
		for my $d (sort {$b cmp $a} keys %$h) {
			OpenBSD::ProgressMeter::show($done, $total);
			my $realname = $state->{destdir}.$d;
			if ($remaining->{dirs}->{$realname}) {
				for my $i (@{$h->{$d}}) {
					$state->set_pkgname($i->{pkgname});
					$i->reload($state);
				}
			} else {
				for my $i (@{$h->{$d}}) {
					$state->set_pkgname($i->{pkgname});
					$i->cleanup($state);
				}
				if (!rmdir $realname) {
					$state->print("Error deleting directory $realname: $!\n")
					    unless $state->{dirs_okay}->{$d};
				}
			}
			$done++;
		}
	}
	if (defined $u) {
		while (my ($user, $pkgname) = each %$u) {
			OpenBSD::ProgressMeter::show($done, $total);
			next if $remaining->{users}->{$user};
			if ($state->{extra}) {
				System("/usr/sbin/userdel", $user);
			} else {
				$state->set_pkgname($pkgname);
				$state->print("You should also run /usr/sbin/userdel $user\n");
			}
			$done++;
		}
	}
	if (defined $g) {
		while (my ($group, $pkgname) = each %$g) {
			OpenBSD::ProgressMeter::show($done, $total);
			next if $remaining->{groups}->{$group};
			if ($state->{extra}) {
				System("/usr/sbin/groupdel", $group);
			} else {
				$state->set_pkgname($pkgname);
				$state->print("You should also run /usr/sbin/groupdel $group\n");
			}
			$done++;
		}
	}
	OpenBSD::ProgressMeter::next();
}

package OpenBSD::PackingElement;
sub record_shared_item
{
}

sub cleanup
{
}

sub reload
{
}

package OpenBSD::PackingElement::NewUser;
sub record_shared_item
{
	my ($self, $pkgname, $db) = @_;
	my $k = $self->{name};
	$db->{users}->{$k} = $pkgname;
}

package OpenBSD::PackingElement::NewGroup;
sub record_shared_item
{
	my ($self, $pkgname, $db) = @_;
	my $k = $self->{name};
	$db->{groups}->{$k} = $pkgname;
}

package OpenBSD::PackingElement::DirBase;
sub record_shared_item
{
	my ($self, $pkgname, $db) = @_;
	my $k = $self->fullname;
	$db->{dirs}->{$k} = 1;
}

package OpenBSD::PackingElement::DirRm;
sub record_shared_item
{
	&OpenBSD::PackingElement::DirBase::record_shared_item;
}

package OpenBSD::PackingElement::Mandir;
sub cleanup
{
	my ($self, $state) = @_;
	my $fullname = $state->{destdir}.$self->fullname;
	$state->print("You may wish to remove ", $fullname, " from man.conf\n");
	unlink("$fullname/whatis.db");
}

package OpenBSD::PackingElement::Fontdir;
sub cleanup
{
	my ($self, $state) = @_;
	my $fullname = $state->{destdir}.$self->fullname;
	$state->print("You may wish to remove ", $fullname, " from your font path\n");
	unlink("$fullname/fonts.alias");
	unlink("$fullname/fonts.dir");
	unlink("$fullname/fonts.cache-1");
}

package OpenBSD::PackingElement::Infodir;
sub cleanup
{
	my ($self, $state) = @_;
	my $fullname = $state->{destdir}.$self->fullname;
	unlink("$fullname/dir");
}

1;
