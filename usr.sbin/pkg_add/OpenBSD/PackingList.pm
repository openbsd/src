# ex:ts=8 sw=4:
# $OpenBSD: PackingList.pm,v 1.111 2010/12/24 09:04:14 espie Exp $
#
# Copyright (c) 2003-2010 Marc Espie <espie@openbsd.org>
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

package OpenBSD::PackingList::hashpath;
sub match
{
	my ($h, $plist) = @_;
	return
	    defined $plist->fullpkgpath &&
	    $h->{$plist->fullpkgpath};
}

package OpenBSD::Composite;

sub AUTOLOAD
{
	our $AUTOLOAD;
	my $fullsub = $AUTOLOAD;
	(my $sub = $fullsub) =~ s/.*:://o;
	return if $sub eq 'DESTROY'; # special case
	my $self = $_[0];
	# verify it makes sense
	if ($self->element_class->can($sub)) {
		no strict "refs";
		# create the sub to avoid regenerating further calls
		*$fullsub = sub {
			my $self = shift;
			$self->visit($sub, @_);
		};
		# and jump to it
		goto &$fullsub;
	} else {
		die "Can't call $sub on ".ref($self);
	}
}

package OpenBSD::PackingList;
our @ISA = qw(OpenBSD::Composite);

use OpenBSD::PackingElement;
use OpenBSD::PackageInfo;

sub element_class { "OpenBSD::PackingElement" }

sub new
{
	my $class = shift;
	my $plist = bless {state => OpenBSD::PackingList::State->new,
		infodir => \(my $d)}, $class;
	OpenBSD::PackingElement::File->add($plist, CONTENTS);
	return $plist;
}

sub set_infodir
{
	my ($self, $dir) = @_;
	$dir .= '/' unless $dir =~ m/\/$/o;
	${$self->{infodir}} = $dir;
}

sub make_shallow_copy
{
	my ($plist, $h) = @_;

	my $copy = bless {state => OpenBSD::PackingList::State->new,
		infodir => \(my $d = ${$plist->{infodir}})}, ref($plist);
	$plist->copy_shallow_if($copy, $h);
	return $copy;
}

sub make_deep_copy
{
	my ($plist, $h) = @_;

	my $copy = bless {state => OpenBSD::PackingList::State->new,
		infodir => \(my $d = ${$plist->{infodir}})}, ref($plist);
	$plist->copy_deep_if($copy, $h);
	return $copy;
}

sub infodir
{
	my $self = shift;
	return ${$self->{infodir}};
}

sub conflict_list
{
	require OpenBSD::PkgCfl;

	my $self = shift;

	$self->{conflict_list} //= OpenBSD::PkgCfl->make_conflict_list($self);
	return $self->{conflict_list};
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
			my $line = shift;
			return if $line =~ m/^\s*$/o;
			OpenBSD::PackingElement->create($line, $plist);
		});
	return $plist;
}

sub defaultCode
{
	my ($fh, $cont) = @_;
	my $_;
	while (<$fh>) {
		&$cont($_);
	}
}

sub SharedItemsOnly
{
	my ($fh, $cont) = @_;
	my $_;
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
	my $_;
	while (<$fh>) {
		next unless m/^\@(?:cwd|lib|name)\b/o ||
			m/^\@comment\s+subdir\=/o;
		&$cont($_);
	}
}

sub FilesOnly
{
	my ($fh, $cont) = @_;
	my $_;
	while (<$fh>) {
	    	next unless m/^\@(?:cwd|name|info|man|file|lib|shell|sample|bin|rcscript)\b/o || !m/^\@/o;
		&$cont($_);
	}
}

sub PrelinkStuffOnly
{
	my ($fh, $cont) = @_;
	my $_;
	while (<$fh>) {
		next unless m/^\@(?:cwd|bin|lib|name|depend|wantlib)\b/o ||
			m/^\@comment\s+subdir\=/o;
		&$cont($_);
	}
}

sub DependOnly
{
	my ($fh, $cont) = @_;
	my $_;
	while (<$fh>) {
		# XXX optimization
		if (m/^\@arch\b/o) {
			while (<$fh>) {
			    if (m/^\@(?:depend|wantlib|define-tag)\b/o) {
				    &$cont($_);
			    } elsif (m/^\@(?:newgroup|newuser|cwd)\b/o) {
				    last;
			    }
			}
			return;
		}
		next unless m/^\@(?:depend|wantlib|define-tag)\b/o;
		&$cont($_);
	}
}

sub ExtraInfoOnly
{
	my ($fh, $cont) = @_;
	my $_;
	while (<$fh>) {
		# XXX optimization
		if (m/^\@arch\b/o) {
			while (<$fh>) {
			    if (m/^\@(?:pkgpath)\b/o) {
				    &$cont($_);
			    } elsif (m/^\@(?:newgroup|newuser|cwd)\b/o) {
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
	my $_;
	while (<$fh>) {
		# XXX optimization
		if (m/^\@arch\b/o) {
			while (<$fh>) {
			    if (m/^\@(?:depend|wantlib|conflict|option|pkgpath|url)\b/o) {
				    &$cont($_);
			    } elsif (m/^\@(?:newgroup|newuser|cwd)\b/o) {
				    last;
			    }
			}
			return;
		}
		# if alwaysupdate, all info is sig
		if (m/^\@option\s+always-update\b/o) {
		    &$cont($_);
		    while (<$fh>) {
			    &$cont($_);
		    }
		    return;
		}
		next unless m/^\@(?:name\b|depend\b|wantlib\b|conflict|\b|option\b|pkgpath\b|comment\s+subdir\=|arch\b|url\b)/o;
		&$cont($_);
	}
}

sub FatOnly
{
	my ($fh, $cont) = @_;
	my $_;
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
	my $_;
	while (<$fh>) {
		# XXX optimization
		if (m/^\@arch\b/o) {
			while (<$fh>) {
			    if (m/^\@(?:conflict|option|name)\b/o) {
				    &$cont($_);
			    } elsif (m/^\@(?:depend|wantlib|newgroup|newuser|cwd)\b/o) {
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
	my $_;
MAINLOOP:
	while (<$fh>) {
		if (m/^\@shared\b/o) {
			&$cont($_);
			while(<$fh>) {
				redo MAINLOOP unless m/^\@(?:sha|md5|size|symlink|link)\b/o;
				    m/^\@size\b/o || m/^\@symlink\b/o ||
				    m/^\@link\b/o;
				&$cont($_);
			}
		} else {
			next unless m/^\@(?:cwd|name)\b/o;
		}
		&$cont($_);
	}
}

sub fromfile
{
	my ($a, $fname, $code) = @_;
	open(my $fh, '<', $fname) or return;
	my $plist;
	eval {
		$plist = $a->read($fh, $code);
	};
	if ($@) {
		chomp $@;
		$@ =~ s/\.$/,/o;
		die "$@ in $fname, ";
	}
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

sub save
{
	my $self = shift;
	$self->tofile($self->infodir.CONTENTS);
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
		die "Duplicate $category in plist ".($plist->pkgname // "?");
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

sub set_pkgname
{
	my ($self, $name) = @_;
	if (defined $self->{name}) {
		$self->{name}->set_name($name);
	} else {
		OpenBSD::PackingElement::Name->add($self, $name);
	}
}

sub pkgname
{
	my $self = shift;
	if (defined $self->{name}) {
		return $self->{name}->name;
	} else {
		return undef;
	}
}

sub localbase
{
	my $self = shift;

	if (defined $self->{localbase}) {
		return $self->{localbase}->name;
	} else {
		return '/usr/local';
	}
}

sub is_signed
{
	my $self = shift;
	return defined $self->{'digital-signature'};
}

sub fullpkgpath
{
	my $self = shift;
	if (defined $self->{extrainfo} && $self->{extrainfo}->{subdir} ne '') {
		return $self->{extrainfo}->{subdir};
	} else {
		return undef;
	}
}
sub pkgpath
{
	my $self = shift;
	if (!defined $self->{_hashpath}) {
		my $h = $self->{_hashpath} =
		    bless {}, "OpenBSD::PackingList::hashpath";
		if (defined $self->fullpkgpath) {
			$h->{$self->fullpkgpath} = 1;
		}
		if (defined $self->{pkgpath}) {
			for my $i (@{$self->{pkgpath}}) {
				$h->{$i->name} = 1;
			}
		}
	}
	return $self->{_hashpath};
}

sub match_pkgpath
{
	my ($self, $plist2) = @_;
	return $self->pkgpath->match($plist2) ||
	    $plist2->pkgpath->match($self);
}

our @unique_categories =
    (qw(name url digital-signature no-default-conflict manual-installation always-update explicit-update extrainfo localbase arch));

our @list_categories =
    (qw(conflict pkgpath incompatibility ask-update updateset depend
    	wantlib define-tag groups users items));

our @cache_categories =
    (qw(depend wantlib));

sub visit
{
	my ($self, $method, @l) = @_;

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
}

my $plist_cache = {};

sub from_installation
{
	my ($o, $pkgname, $code) = @_;

	require OpenBSD::PackageInfo;

	$code //= \&defaultCode;

	if ($code == \&DependOnly && defined $plist_cache->{$pkgname}) {
	    return $plist_cache->{$pkgname};
	}
	my $filename = OpenBSD::PackageInfo::installed_contents($pkgname);
	my $plist = $o->fromfile($filename, $code);
	if (defined $plist && $code == \&DependOnly) {
		$plist_cache->{$pkgname} = $plist;
	}
	if (defined $plist) {
		$plist->set_infodir(OpenBSD::PackageInfo::installed_info($pkgname));
	}
	if (!defined $plist) {
		print STDERR "Warning: couldn't read packing-list from installed package $pkgname\n";
		unless (-e $filename) {
			print STDERR "File $filename does not exist\n";
		}
	}
	return $plist;
}

sub to_cache
{
	my ($self) = @_;
	return if defined $plist_cache->{$self->pkgname};
	my $plist = new OpenBSD::PackingList;
	for my $c (@cache_categories) {
		if (defined $self->{$c}) {
			$plist->{$c} = $self->{$c};
		}
	}
	$plist_cache->{$self->pkgname} = $plist;
}

sub to_installation
{
	my ($self) = @_;

	require OpenBSD::PackageInfo;

	return if $main::not;

	$self->tofile(OpenBSD::PackageInfo::installed_contents($self->pkgname));
}


sub forget
{
}

# convert call to $self->sub(@args) into $self->visit(sub, @args)

sub signature
{
	my $self = shift;

	require OpenBSD::Signature;
	return OpenBSD::Signature->from_plist($self);
}

1;
