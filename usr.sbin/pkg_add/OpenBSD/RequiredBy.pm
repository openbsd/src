# ex:ts=8 sw=4:
# $OpenBSD: RequiredBy.pm,v 1.5 2004/11/14 11:40:08 espie Exp $
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

package OpenBSD::RequirementList;
use OpenBSD::PackageInfo;

sub list($)
{
	my $self = shift;

	if (wantarray) {
		return () unless -f $$self;
		open(my $fh, '<', $$self) or 
		    die "Problem opening required list: $$self: $!";
		local $_;
		my $l = {};
		while(<$fh>) {
			chomp $_;
			s/\s+$//;
			next if /^$/;
			$l->{$_} = 1;
		}
		close($fh);
		return keys %$l;
	} else {
		return -f $$self ? 1 : 0;
	}
}

sub delete
{
	my ($self, $pkgname) = @_;
	my @lines = grep { $_ ne $pkgname } $self->list();
	unlink($$self) or die "Can't erase $$self: $!";
	if (@lines > 0) {
		$self->add(@lines);
	} 
}

sub add
{
	my ($self, @pkgnames) = @_;
	open(my $fh, '>>', $$self) or
	    die "Can't add dependencies to $$self: $!";
	print $fh join("\n", @pkgnames), "\n";
	close($fh);
}

sub new
{
	my ($class, $pkgname) = @_;
	my $f = installed_info($pkgname).$class->filename();
	bless \$f, $class;
}

package OpenBSD::RequiredBy;
our @ISA=qw(OpenBSD::RequirementList);
use OpenBSD::PackageInfo;

sub filename() { REQUIRED_BY };

package OpenBSD::Requiring;
our @ISA=qw(OpenBSD::RequirementList);
use OpenBSD::PackageInfo;

sub filename() { REQUIRING };

1;
