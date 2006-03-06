# ex:ts=8 sw=4:
# $OpenBSD: SCP.pm,v 1.1 2006/03/06 10:40:32 espie Exp $
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

package OpenBSD::PackageRepository::SCP;
our @ISA=qw(OpenBSD::PackageRepository::Distant);


sub grab_object
{
	my ($self, $object) = @_;

	exec {"/usr/bin/scp"} 
	    "scp", 
	    $self->{host}.":".$self->{path}.$object->{name}, 
	    "/dev/stdout"
	or die "can't run scp";
}

our %distant = ();

sub maxcount
{
	return 2;
}

sub opened
{
	my $self = $_[0];
	my $k = $self->{key};
	if (!defined $distant{$k}) {
		$distant{$k} = [];
	}
	return $distant{$k};
}

sub _new
{
	my ($class, $baseurl) = @_;
	$baseurl =~ s/scp\:\/\///i;
	$baseurl =~ m/\//;
	bless {	host => $`, key => $`, path => "/$'" }, $class;
}

sub list
{
	my ($self) = @_;
	if (!defined $self->{list}) {
		my $host = $self->{host};
		my $path = $self->{path};
		$self->{list} = $self->_list("ssh $host ls -l $path");
	}
	return $self->{list};
}

1;
