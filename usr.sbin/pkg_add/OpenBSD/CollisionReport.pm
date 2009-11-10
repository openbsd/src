# ex:ts=8 sw=4:
# $OpenBSD: CollisionReport.pm,v 1.20 2009/11/10 11:36:56 espie Exp $
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
use OpenBSD::Vstat;

sub find_collisions
{
	my ($todo, $verbose) = @_;
	my $bypkg = {};
	for my $name (keys %$todo) {
		my $p = OpenBSD::Vstat::vexists $name;
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
		print "Looking for collisions in $pkg\n" if $verbose;
		my $plist = OpenBSD::PackingList->from_installation($pkg, 
		    \&OpenBSD::PackingList::FilesOnly);
		next if !defined $plist;
		for my $item (@{$plist->{items}}) {
			next unless $item->IsFile;
			my $name = $item->fullname;
			if (defined $todo->{$name}) {
				push(@{$bypkg->{$pkg}}, $name);
				delete $todo->{$name};
				return $bypkg;
			}
		}
	}
	return $bypkg;
}

sub collision_report($$)
{
	my ($list, $state) = @_;

	if ($state->{defines}->{removecollisions}) {
		require OpenBSD::Error;
		for my $f (@$list) {
			OpenBSD::Error::Unlink(1, $f->fullname);
		}
		return;
	}
	my %todo = map {($_->fullname, $_->{d})} @$list;
	my $clueless_bat;
	my $clueless_bat2;
	my $found = 0;
	
	print "Collision: the following files already exist\n";
	if (!$state->{defines}->{dontfindcollisions}) {
		my $bypkg = find_collisions(\%todo, $state->{verbose});
		for my $pkg (sort keys %$bypkg) {
		    for my $item (sort @{$bypkg->{$pkg}}) {
		    	$found++;
			print "\t$item ($pkg)\n";
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
		my $destdir = $state->{destdir};

		for my $item (sort keys %todo) {
		    if (defined $todo{$item}) {
			    my $old = $todo{$item};
			    my $d = $old->new($destdir.$item);
			    if ($d->equals($old)) {
				print "\t$item (same checksum)\n";
			    } else {
				print "\t$item (different checksum)\n";
			    }
		    } else {
			    print "\t$item\n";
		    }
	    	}
	}
	if (defined $clueless_bat) {
		print "The package name $clueless_bat suggests that a former installation\n";
		print "of a similar package got interrupted.  It is likely that\n";
		print "\tpkg_delete $clueless_bat\n";
		print "will solve the problem\n";
	}
	if (defined $clueless_bat2) {
		print "The package name $clueless_bat2 suggests remaining libraries\n";
		print "from a former package update.  It is likely that\n";
		print "\tpkg_delete $clueless_bat2\n";
		print "will solve the problem\n";
	}
	my $dorepair = 0;
	if ($found == 0) {
		if ($state->{defines}->{repair}) {
			$dorepair = 1;
		} elsif ($state->{interactive}) {
			require OpenBSD::Interactive;
			if (OpenBSD::Interactive::confirm(
	    "It seems to be a missing package registration\nRepair", 1, 0)) {
				$dorepair = 1;
			}
		}
	}
	if ($dorepair == 1) {
		require OpenBSD::Error;
		for my $f (@$list) {

			if (OpenBSD::Error::Unlink($state->{verbose}, 
			    $f->fullname)) {
				$state->{problems}--;
			} else {
				return;
			}
		}
		$state->{repairdependencies} = 1;
	}
}

1;
