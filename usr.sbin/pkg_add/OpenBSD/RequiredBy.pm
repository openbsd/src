# ex:ts=8 sw=4:
# $OpenBSD: RequiredBy.pm,v 1.1.1.1 2003/10/16 17:43:34 espie Exp $
#
# Copyright (c) 2003 Marc Espie.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
# PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

package OpenBSD::RequiredBy;
use strict;
use warnings;
use OpenBSD::PackageInfo;

sub new
{
	my ($class, $pkgname) = @_;
	my $f = installed_info($pkgname).REQUIRED_BY;
	bless \$f, $class;
}

sub list($)
{
	my $self = shift;

	my $l = [];
	return $l unless -f $$self;
	open(my $fh, '<', $$self) or 
	    die "Problem opening required list: $$self\n";
	local $_;
	while(<$fh>) {
		chomp $_;
		s/\s+$//;
		next if /^$/;
		push(@$l, $_);
	}
	close($fh);
	return $l;
}

sub delete
{
	my ($self, $pkgname) = @_;
	my @lines = grep { $_ ne $pkgname } @{$self->list()};
	unlink($$self) or die "Can't erase $$self";
	if (@lines > 0) {
		$self->add(@lines);
	} 
}

sub add
{
	my ($self, @pkgnames) = @_;
	open(my $fh, '>>', $$self) or
	    die "Can't add dependencies to $$self";
	print $fh join("\n", @pkgnames), "\n";
	close($fh);
}

sub DESTROY
{
}

1;
