# ex:ts=8 sw=4:
# $OpenBSD: Update.pm,v 1.67 2007/05/02 15:05:30 espie Exp $
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
#
use strict;
use warnings;

package OpenBSD::Update;
use OpenBSD::ProgressMeter;
use OpenBSD::Interactive;
use OpenBSD::PackageInfo;
use OpenBSD::PackageLocator;
use OpenBSD::PackageName;
use OpenBSD::Error;


sub find
{
	my ($old, $new, $state) = @_;
	my @list = ();

	OpenBSD::PackageInfo::solve_installed_names($old, \@list, "(updating them all)", $state);
	unless (defined $state->{full_update} or defined $state->{forced}->{noclosure}) {
		require OpenBSD::RequiredBy;

		@list = OpenBSD::Requiring->compute_closure(@list);
	}
	my @cantupdate = ();
	my $hash = OpenBSD::PackageName::available_stems($state);

	OpenBSD::ProgressMeter::set_header("Looking for updates");
	for my $pkgname (@list) {
		if ($pkgname =~ m/^(?:\.libs|partial)\-/) {
			OpenBSD::ProgressMeter::clear();
			print "Not updating $pkgname, remember to clean it\n";
			next;
		}
		my $stem = OpenBSD::PackageName::splitstem($pkgname);
		my @l = $hash->findstem($stem);
		if (@l == 0) {
			push(@cantupdate, $pkgname);
			next;
		}
		my @l2 = ();
		if (@l == 1 && $state->{forced}->{pkgpath}) {
			OpenBSD::ProgressMeter::clear();
			print "Directly updating $pkgname -> ", $l[0], "\n";
			push(@$new, $l[0]);
			next;
		}
		my $plist = OpenBSD::PackingList->from_installation($pkgname, \&OpenBSD::PackingList::UpdateInfoOnly);
		if (!defined $plist) {
			Fatal("Can't locate $pkgname");
		}
		my $found;
		for my $candidate (@l) {
		    my $handle = OpenBSD::PackageLocator->find($candidate, $state->{arch});
		    if (!$handle) {
			    next;
		    }
		    $handle->close_now();
		    my $p2 = $handle->plist(\&OpenBSD::PackingList::UpdateInfoOnly);
		    if (!$p2) {
		    	next;
		    }
		    if ($p2->has('arch')) {
			unless ($p2->{arch}->check($state->{arch})) {
			    next;
			}
		    }
		    if ($plist->signature() eq $p2->signature()) {
		    	$found = $candidate;
		    }
		    if ($p2->{extrainfo}->{subdir} eq $plist->{extrainfo}->{subdir}) {
			push(@l2, $candidate);
		    } elsif ($p2->has('pkgpath')) {
		    	for my $p (@{$p2->{pkgpath}}) {
				if ($p->{name} eq $plist->{extrainfo}->{subdir}) {
					push(@l2, $candidate);
					last;
				}
		    	}
		    }
		}

		if (defined $found && @l2 == 1 && $found eq  $l2[0]) {
			if (!$plist->uses_old_libs) {
				my $msg = "No need to update $pkgname";
				OpenBSD::ProgressMeter::message($msg);
				print "$msg\n" if $state->{beverbose};
				next;
			}
		}
		OpenBSD::ProgressMeter::clear();
		print "Candidates for updating $pkgname -> ", join(' ', @l2), "\n";
		# if all packages have the same version, but distinct p,
		# grab the most recent.
		if (@l2 > 1) {
		    @l2 = OpenBSD::PackageName::keep_most_recent(@l2);
		}
			
		if (@l2 == 1) {
			if (defined $found && $found eq  $l2[0] && !$plist->uses_old_libs) {
				my $msg = "No need to update $pkgname";
				OpenBSD::ProgressMeter::message($msg);
				print "$msg\n" if $state->{beverbose};
			} else {
				push(@$new, $l2[0]);
			}
		} elsif (@l2 == 0) {
			push(@cantupdate, $pkgname);
		} else {
			my $result = OpenBSD::Interactive::choose1($pkgname, $state->{interactive}, sort @l2);
			if (defined $result) {
				if (defined $found && $found eq  $result && !$plist->uses_old_libs) {
					print "No need to update $pkgname\n";
				} else {
					push(@$new, $result);
				}
			} else {
				$state->{issues} = 1;
			}
		}
	}
	OpenBSD::ProgressMeter::next();
	return @cantupdate;
}

1;
