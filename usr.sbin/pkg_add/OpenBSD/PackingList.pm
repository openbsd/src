# ex:ts=8 sw=4:
# $OpenBSD: PackingList.pm,v 1.28 2004/10/11 14:25:28 espie Exp $
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
package OpenBSD::PackingList;

use OpenBSD::PackingElement;
use OpenBSD::PackageInfo;

sub new
{
	my $class = shift;
	bless {state => 
	    {default_owner=>'root', 
	     default_group=>'bin', 
	     default_mode=> 0444,
	     cwd=>'.'} }, $class;
}

sub read
{
	my ($a, $fh, $code) = @_;
	my $plist;
	if (ref $a) {
		$plist = $a;
	} else {
		$plist = new $a;
	}
	$code = \&defaultCode if !defined $code;
	&$code($fh,
		sub {
			local $_ = shift;
			next if m/^\s*$/;
			chomp;
			OpenBSD::PackingElement::Factory($_, $plist);
		});
	return $plist;
}

sub defaultCode
{
	my ($fh, $cont) = @_;
	local $_;
	while (<$fh>) {
		&$cont($_);
	}
}

sub DirrmOnly
{
	my ($fh, $cont) = @_;
	local $_;
	while (<$fh>) {
		next unless m/^\@(?:cwd|dirrm|dir|fontdir|mandir|newuser|newgroup|name)\b/ || m/^\@(?:sample|extra)\b.*\/$/ || m/^[^\@].*\/$/;
		&$cont($_);
	}
}

sub LibraryOnly
{
	my ($fh, $cont) = @_;
	local $_;
	while (<$fh>) {
		next unless m/^\@(?:cwd|lib|name)\b/ ||
			m/^\@comment\s+subdir\=/;
		&$cont($_);
	}
}

sub FilesOnly
{
	my ($fh, $cont) = @_;
	local $_;
	while (<$fh>) {
	    	next unless m/^\@(?:cwd|name|info|man|file|lib|shell)\b/ || !m/^\@/;
		&$cont($_);
	}
}

sub ConflictOnly
{
	my ($fh, $cont) = @_;
	local $_;
	while (<$fh>) {
	    	next unless m/^\@(?:pkgcfl|conflict|option|name)\b/;
		&$cont($_);
	}
}

sub SharedStuffOnly
{
	my ($fh, $cont) = @_;
	local $_;
MAINLOOP:
	while (<$fh>) {
		if (m/^\@shared\b/) {
			&$cont($_);
			while(<$fh>) {
				redo MAINLOOP unless m/^\@(?:md5|size|symlink|link)\b/;
				    m/^\@size\b/ || m/^\@symlink\b/ || 
				    m/^\@link\b/;
				&$cont($_);
			}
		} else {
			next unless m/^\@(?:cwd|dirrm|name)\b/;
		}
		&$cont($_);
	}
}

sub write
{
	my ($self, $fh) = @_;

	$self->visit('write', $fh);

}


sub fromfile
{
	my ($a, $fname, $code) = @_;
	open(my $fh, '<', $fname) or return undef;
	my $plist = $a->read($fh, $code);
	close($fh);
	return $plist;
}

sub tofile
{
	my ($self, $fname) = @_;
	open(my $fh, '>', $fname) or return undef;
	$self->write($fh);
	close($fh) or return undef;
	return 1;
}

sub add2list
{
	my ($plist, $object) = @_;
	my $category = $object->category();
	$plist->{$category} = [] unless defined $plist->{$category};
	push @{$plist->{$category}}, $object;
}

sub addunique
{
	my ($plist, $object) = @_;
	my $category = $object->category();
	if (defined $plist->{$category}) {
		die "Duplicate $category in plist\n";
	}
	$plist->{$category} = $object;
}

sub has
{
	my ($plist, $name) = @_;
	return defined $plist->{$name};
}

sub get
{
	my ($plist, $name) = @_;
	return $plist->{$name};
}

sub pkgname($)
{
	my $self = shift;
	return $self->{name}->{name};
}

sub pkgbase($)
{
	my $self = shift;

	if (defined $self->{localbase}) {
		return $self->{localbase}->{name};
	} else {
		return '/usr/local';
	}
}


{
    package OpenBSD::PackingList::Visitor;
    sub new
    {
	    my $class = shift;
	    bless {list=>[], pass=>1}, $class;
    }

    sub revisit
    {
	    my ($self, $item) = @_;
	    push(@{$self->{list}}, $item);
    }
}

sub visit
{
	my ($self, $method, @l) = @_;

	my $visitor = new OpenBSD::PackingList::Visitor;

	push(@l, $visitor);

	if (defined $self->{cvstags}) {
		for my $item (@{$self->{cvstags}}) {
			$item->$method(@l);
		}
	}

	for my $unique_item (qw(name no-default-conflict manual-installation extrainfo arch)) {
		$self->{$unique_item}->$method(@l) if defined $self->{$unique_item};
	}

	for my $special (OpenBSD::PackageInfo::info_names()) {
		$self->{$special}->$method(@l) if defined $self->{$special};
	}

	for my $listname (qw(modules pkgcfl conflict pkgdep newdepend libdepend groups users items)) {
		if (defined $self->{$listname}) {
			for my $item (@{$self->{$listname}}) {
				$item->$method(@l);
			}
		}
	}
	$visitor->{pass} = 2;
	while (my $item = shift @{$visitor->{list}}) {
		$item->method(@l);
	}
}

1;
