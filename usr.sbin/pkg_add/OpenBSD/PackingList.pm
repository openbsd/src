# ex:ts=8 sw=4:
# $OpenBSD: PackingList.pm,v 1.63 2007/05/03 14:47:29 espie Exp $
#
# Copyright (c) 2003-2007 Marc Espie <espie@openbsd.org>
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

package OpenBSD::PackingList::State;
my $dot = '.';

sub new
{
	my $class = shift;
	bless { default_owner=>'root', 
	     default_group=>'bin', 
	     default_mode=> 0444,
	     cwd=>\$dot}, $class;
}

sub cwd
{
	return ${$_[0]->{cwd}};
}

sub set_cwd
{
	my ($self, $p) = @_;

	require File::Spec;

	$p = File::Spec->canonpath($p);
	$self->{cwd} = \$p;
}

package OpenBSD::PackingList;

use OpenBSD::PackingElement;
use OpenBSD::PackageInfo;

sub new
{
	my $class = shift;
	bless {state => 
		OpenBSD::PackingList::State->new
	}, $class;
}

sub read
{
	my ($a, $u, $code) = @_;
	my $plist;
	$code = \&defaultCode if !defined $code;
	if (ref $a) {
		$plist = $a;
	} else {
		$plist = new $a;
	}
	&$code($u,
		sub {
			local $_ = shift;
			return if m/^\s*$/;
			chomp;
			OpenBSD::PackingElement->create($_, $plist);
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

sub SharedItemsOnly
{
	my ($fh, $cont) = @_;
	local $_;
	while (<$fh>) {
		next unless m/^\@(?:cwd|dir|fontdir|mandir|newuser|newgroup|name)\b/o || m/^\@(?:sample|extra)\b.*\/$/o || m/^[^\@].*\/$/o;
		&$cont($_);
	}
}

sub DirrmOnly
{
	&OpenBSD::PackingList::SharedItemsOnly;
}

sub LibraryOnly
{
	my ($fh, $cont) = @_;
	local $_;
	while (<$fh>) {
		next unless m/^\@(?:cwd|lib|name)\b/o ||
			m/^\@comment\s+subdir\=/o;
		&$cont($_);
	}
}

sub FilesOnly
{
	my ($fh, $cont) = @_;
	local $_;
	while (<$fh>) {
	    	next unless m/^\@(?:cwd|name|info|man|file|lib|shell)\b/o || !m/^\@/o;
		&$cont($_);
	}
}

sub DependOnly
{
	my ($fh, $cont) = @_;
	local $_;
	while (<$fh>) {
		# XXX optimization
		if (m/^\@arch\b/o) {
			while (<$fh>) {
			    if (m/^\@(?:depend|wantlib)\b/o) {
				    &$cont($_);
			    } elsif (m/^\@(?:groups|users|cwd)\b/o) {
				    last;
			    }
			}
			return;
		}
		next unless m/^\@(?:depend|wantlib)\b/o;
		&$cont($_);
	}
}

sub ExtraInfoOnly
{
	my ($fh, $cont) = @_;
	local $_;
	while (<$fh>) {
		# XXX optimization
		if (m/^\@arch\b/o) {
			while (<$fh>) {
			    if (m/^\@(?:pkgpath)\b/o) {
				    &$cont($_);
			    } elsif (m/^\@(?:groups|users|cwd)\b/o) {
				    last;
			    }
			}
			return;
		}
		next unless m/^\@(?:name\b|comment\s+subdir\=)/o;
		&$cont($_);
	}
}

sub UpdateInfoOnly
{
	my ($fh, $cont) = @_;
	local $_;
	while (<$fh>) {
		# XXX optimization
		if (m/^\@arch\b/o) {
			while (<$fh>) {
			    if (m/^\@(?:depend|wantlib|pkgpath)\b/o) {
				    &$cont($_);
			    } elsif (m/^\@(?:groups|users|cwd)\b/o) {
				    last;
			    }
			}
			return;
		}
		next unless m/^\@(?:name\b|depend\b|wantlib\b|pkgpath\b|comment\s+subdir\=|arch\b)/o;
		&$cont($_);
	}
}

sub FatOnly
{
	my ($fh, $cont) = @_;
	local $_;
	while (<$fh>) {
		# XXX optimization
		if (m/^\@arch\b/o) {
			&$cont($_);
			return;
		}
		next unless m/^\@(?:name\b)/o;
		&$cont($_);
	}
}

sub ConflictOnly
{
	my ($fh, $cont) = @_;
	local $_;
	while (<$fh>) {
		# XXX optimization
		if (m/^\@arch\b/o) {
			while (<$fh>) {
			    if (m/^\@(?:conflict|option|name)\b/o) {
				    &$cont($_);
			    } elsif (m/^\@(?:depend|wantlib|groups|users|cwd)\b/o) {
				    last;
			    }
			}
			return;
		}
	    	next unless m/^\@(?:conflict|option|name)\b/o;
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
			next unless m/^\@(?:cwd|name)\b/;
		}
		&$cont($_);
	}
}

sub fromfile
{
	my ($a, $fname, $code) = @_;
	open(my $fh, '<', $fname) or return;
	my $plist = $a->read($fh, $code);
	close($fh);
	return $plist;
}

sub tofile
{
	my ($self, $fname) = @_;
	open(my $fh, '>', $fname) or return;
	$self->write($fh);
	close($fh) or return;
	return 1;
}

sub add2list
{
	my ($plist, $object) = @_;
	my $category = $object->category;
	push @{$plist->{$category}}, $object;
}

sub addunique
{
	my ($plist, $object) = @_;
	my $category = $object->category;
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

sub pkgname
{
	my $self = shift;
	return $self->{name}->{name};
}

sub localbase
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

our @unique_categories =
    (qw(name no-default-conflict manual-installation extrainfo localbase arch));

our @list_categories =
    (qw(conflict pkgpath incompatibility updateset depend 
    	wantlib module groups users items));

our @cache_categories =
    (qw(depend wantlib));
	
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

	for my $unique_item (@unique_categories) {
		$self->{$unique_item}->$method(@l) if defined $self->{$unique_item};
	}

	for my $special (OpenBSD::PackageInfo::info_names()) {
		$self->{$special}->$method(@l) if defined $self->{$special};
	}

	for my $listname (@list_categories) {
		if (defined $self->{$listname}) {
			for my $item (@{$self->{$listname}}) {
				$item->$method(@l);
			}
		}
	}
	$visitor->{pass} = 2;
	while (my $item = shift @{$visitor->{list}}) {
		$item->$method(@l);
	}
}

my $plist_cache = {};

sub from_installation
{
	my ($o, $pkgname, $code) = @_;

	require OpenBSD::PackageInfo;

	$code = \&defaultCode if !defined $code;

	if ($code == \&DependOnly && defined $plist_cache->{$pkgname}) {
	    return $plist_cache->{$pkgname};
	}
	my $plist =
	    $o->fromfile(OpenBSD::PackageInfo::installed_contents($pkgname), 
		$code);
	if (defined $plist && $code == \&DependOnly) {
		$plist_cache->{$pkgname} = $plist;
	} 
	return $plist;
}

sub to_cache
{
	my ($self) = @_;
	return if defined $plist_cache->{$self->pkgname()};
	my $plist = new OpenBSD::PackingList;
	for my $c (@cache_categories) {
		if (defined $self->{$c}) {
			$plist->{$c} = $self->{$c};
		}
	}
	$plist_cache->{$self->pkgname()} = $plist;
}

sub to_installation
{
	my ($self) = @_;

	require OpenBSD::PackageInfo;

	return if $main::not;

	$self->tofile(OpenBSD::PackageInfo::installed_contents($self->pkgname));
}


sub signature
{
	my $self = shift;
	my $k = {};
	$self->visit('signature', $k);
	return join(',', $self->pkgname, sort keys %$k);
}

sub forget
{
}

# convert call to $self->sub(@args) into $self->visit(sub, @args)

sub AUTOLOAD 
{
	our $AUTOLOAD;
	my $fullsub = $AUTOLOAD;
	(my $sub = $fullsub) =~ s/.*:://;
	return if $sub eq 'DESTROY'; # special case
	# verify it makes sense
	if (OpenBSD::PackingElement->can($sub)) {
		no strict "refs";
		# create the sub to avoid regenerating further calls
		*$fullsub = sub {
			my $self = shift;
			$self->visit($sub, @_);
		};
		# and jump to it
		goto &$fullsub;
	} else {
		die "Can't call $sub on ", __PACKAGE__;
	}
}

1;
