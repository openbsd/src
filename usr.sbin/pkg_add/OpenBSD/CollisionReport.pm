# ex:ts=8 sw=4:
# $OpenBSD: CollisionReport.pm,v 1.3 2004/11/14 19:50:44 espie Exp $
#
# Copyright (c) 2003-2004 Marc Espie <espie@openbsd.org>
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


sub collision_report($$)
{
	my ($list, $state) = @_;
	my %todo = map {($_->fullname(), $_->{md5})} @$list;
	my $bypkg = {};
	

	BIGLOOP: {
	for my $pkg (installed_packages()) {
		print "Looking for collisions in $pkg\n" if $state->{verbose};
		my $plist = OpenBSD::PackingList->from_installation($pkg, 
		    \&OpenBSD::PackingList::FilesOnly);
		for my $item (@{$plist->{items}}) {
			next unless $item->IsFile();
			my $name = $item->fullname();
			if (defined $todo{$name}) {
				$bypkg->{$pkg} = [] unless defined $bypkg->{$pkg};
				push(@{$bypkg->{$pkg}}, $name);
				delete $todo{$name};
				last BIGLOOP if !%todo;
			}
		}
	}
	}
	print "Collision: the following files already exist\n";
	for my $pkg (sort keys %$bypkg) {
	    for my $item (sort @{$bypkg->{$pkg}}) {
	    	print "\t$item ($pkg)\n";
	    }
	}
	if (%todo) {
		require OpenBSD::md5;
		my $destdir = $state->{destdir};

		for my $item (sort keys %todo) {
		    my $md5 = OpenBSD::md5::fromfile($destdir.$item);
		    if ($md5 eq $todo{$item}) {
			print "\t$item (same md5)\n";
		    } else {
			print "\t$item (different md5)\n";
		    }
	    	}
	}
}

1;
