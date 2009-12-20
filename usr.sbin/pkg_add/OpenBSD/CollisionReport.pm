# ex:ts=8 sw=4:
# $OpenBSD: CollisionReport.pm,v 1.29 2009/12/20 22:38:45 espie Exp $
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

package OpenBSD::CollisionReport;
use OpenBSD::PackingList;
use OpenBSD::PackageInfo;

sub find_collisions
{
	my ($todo, $state) = @_;
	my $verbose = $state->verbose >= 3;
	my $bypkg = {};
	for my $name (keys %$todo) {
		my $p = $state->vstat->exists($name);
		if (ref $p) {
			my $pkg = $$p;
			push(@{$bypkg->{$pkg}}, $name);
			delete $todo->{$name};
		}
	}


	if (!%$todo) {
		return $bypkg;
	}
	for my $pkg (installed_packages()) {
		$state->say("Looking for collisions in $pkg") if $verbose;
		my $plist = OpenBSD::PackingList->from_installation($pkg, 
		    \&OpenBSD::PackingList::FilesOnly);
		next if !defined $plist;
		for my $item (@{$plist->{items}}) {
			next unless $item->IsFile;
			my $name = $item->fullname;
			if (defined $todo->{$name}) {
				push(@{$bypkg->{$pkg}}, $name);
				delete $todo->{$name};
			}
		}
	}
	return $bypkg;
}

sub collision_report($$)
{
	my ($list, $state) = @_;

	my $destdir = $state->{destdir};

	if ($state->{defines}->{removecollisions}) {
		require OpenBSD::Error;
		for my $f (@$list) {
			$state->unlink(1, $destdir.$f->fullname);
		}
		return;
	}
	my %todo = map {($_->fullname, $_->{d})} @$list;
	my $clueless_bat;
	my $clueless_bat2;
	my $found = 0;
	
	$state->errsay("Collision: the following files already exist");
	if (!$state->{defines}->{dontfindcollisions}) {
		my $bypkg = find_collisions(\%todo, $state);
		for my $pkg (sort keys %$bypkg) {
		    for my $item (sort @{$bypkg->{$pkg}}) {
		    	$found++;
			$state->errsay("\t$item ($pkg)");
		    }
		    if ($pkg =~ m/^(?:partial\-|borked\.\d+$)/o) {
			$clueless_bat = $pkg;
		    }
		    if ($pkg =~ m/^\.libs\d*-*$/o) {
			$clueless_bat2 = $pkg;
		    }
		}
	}
	if (%todo) {

		for my $item (sort keys %todo) {
		    if (defined $todo{$item}) {
			    my $old = $todo{$item};
			    my $d = $old->new($destdir.$item);
			    if ($d->equals($old)) {
				$state->errsay("\t$item (same checksum)");
			    } else {
				$state->errsay("\t$item (different checksum)");
			    }
		    } else {
			    $state->errsay("\t$item");
		    }
	    	}
	}
	if (defined $clueless_bat) {
		$state->errprint("The package name $clueless_bat suggests that a former installation\n",
		    "of a similar package got interrupted.  It is likely that\n",
		    "\tpkg_delete $clueless_bat\n",
		    "will solve the problem\n");
	}
	if (defined $clueless_bat2) {
		$state->errprint("The package name $clueless_bat2 suggests remaining libraries\n",
		    "from a former package update.  It is likely that\n",
		    "\tpkg_delete $clueless_bat2\n".
		    "will solve the problem\n");
	}
	my $dorepair = 0;
	if ($found == 0) {
		if ($state->{defines}->{repair}) {
			$dorepair = 1;
		} elsif ($state->{interactive}) {
			if ($state->confirm("It seems to be a missing package registration\nRepair", 0)) {
				$dorepair = 1;
			}
		}
	}
	if ($dorepair == 1) {
		for my $f (@$list) {

			if ($state->unlink($state->verbose >= 2, 
			    $destdir.$f->fullname)) {
				$state->{problems}--;
			} else {
				return;
			}
		}
		$state->{repairdependencies} = 1;
	}
}

1;
