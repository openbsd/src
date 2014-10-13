# ex:ts=8 sw=4:
# $OpenBSD: PackingList.pm,v 1.135 2014/10/13 12:44:16 espie Exp $
#
# Copyright (c) 2003-2014 Marc Espie <espie@openbsd.org>
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
	     owners => {},
	     groups => {},
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
	my $f = $plist->fullpkgpath2;
	if (!defined $f) {
		return 0;
	}
	for my $i (@{$h->{$f->{dir}}}) {
		if ($i->match($f)) {
			return 1;
		}
	}
	return 0;
}

package OpenBSD::Composite;

# convert call to $self->sub(@args) into $self->visit(sub, @args)
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

	my $copy = ref($plist)->new;
	$copy->set_infodir($plist->infodir);
	$plist->copy_shallow_if($copy, $h);
	return $copy;
}

sub make_deep_copy
{
	my ($plist, $h) = @_;

	my $copy = ref($plist)->new;
	$copy->set_infodir($plist->infodir);
	$plist->copy_deep_if($copy, $h);
	return $copy;
}

sub infodir
{
	my $self = shift;
	return ${$self->{infodir}};
}

sub zap_wrong_annotations
{
	my $self = shift;
	my $pkgname = $self->pkgname;
	if (defined $pkgname && $pkgname =~ m/^(?:\.libs\d*|partial)\-/) {
		delete $self->{'manual-installation'};
		delete $self->{'firmware'};
		delete $self->{'digital-signature'};
	}
}

sub conflict_list
{
	require OpenBSD::PkgCfl;

	my $self = shift;
	return OpenBSD::PkgCfl->make_conflict_list($self);
}

my $subclass;

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
	if (defined $subclass->{$code}) {
		bless $plist, "OpenBSD::PackingList::".$subclass->{$code};
	}
	&$code($u,
		sub {
			my $line = shift;
			return if $line =~ m/^\s*$/o;
			OpenBSD::PackingElement->create($line, $plist);
		});
	$plist->zap_wrong_annotations;
	return $plist;
}

sub defaultCode
{
	my ($fh, $cont) = @_;
	while (<$fh>) {
		&$cont($_);
	}
}

sub SharedItemsOnly
{
	my ($fh, $cont) = @_;
	while (<$fh>) {
		next unless m/^\@(?:cwd|dir|fontdir|ghost|mandir|newuser|newgroup|name)\b/o || m/^\@(?:sample|extra)\b.*\/$/o || m/^[^\@].*\/$/o;
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
	while (<$fh>) {
		next unless m/^\@(?:cwd|lib|name|comment\s+subdir\=)\b/o;
		&$cont($_);
	}
}

sub FilesOnly
{
	my ($fh, $cont) = @_;
	while (<$fh>) {
	    	next unless m/^\@(?:cwd|name|info|man|file|lib|shell|sample|bin|rcscript)\b/o || !m/^\@/o;
		&$cont($_);
	}
}

sub PrelinkStuffOnly
{
	my ($fh, $cont) = @_;
	while (<$fh>) {
		next unless m/^\@(?:cwd|bin|lib|name|depend|wantlib|comment\s+ubdir\=)\b/o;
		&$cont($_);
	}
}

sub DependOnly
{
	my ($fh, $cont) = @_;
	while (<$fh>) {
		if (m/^\@(?:depend|wantlib|define-tag)\b/o) {
			&$cont($_);
		# XXX optimization
		} elsif (m/^\@(?:newgroup|newuser|cwd)\b/o) {
			last;
		}
	}
}

sub ExtraInfoOnly
{
	my ($fh, $cont) = @_;
	while (<$fh>) {
		if (m/^\@(?:name|pkgpath|comment\s+(?:subdir|pkgpath)\=)\b/o) {
			&$cont($_);
		# XXX optimization
		} elsif (m/^\@(?:depend|wantlib|newgroup|newuser|cwd)\b/o) {
			last;
		}
	}
}

sub UpdateInfoOnly
{
	my ($fh, $cont) = @_;
	while (<$fh>) {
		# if alwaysupdate, all info is sig
		if (m/^\@option\s+always-update\b/o) {
		    &$cont($_);
		    while (<$fh>) {
			    &$cont($_);
		    }
		    return;
		}
		if (m/^\@(?:name|depend|wantlib|conflict|option|pkgpath|url|arch|comment\s+(?:subdir|pkgpath)\=)\b/o) {
			&$cont($_);
		# XXX optimization
		} elsif (m/^\@(?:newgroup|newuser|cwd)\b/o) {
			last;
		}
	}
}

sub ConflictOnly
{
	my ($fh, $cont) = @_;
	while (<$fh>) {
		if (m/^\@(?:name|conflict|option)\b/o) {
			&$cont($_);
		# XXX optimization
		} elsif (m/^\@(?:depend|wantlib|newgroup|newuser|cwd)\b/o) {
			last;
		}
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
	$self->zap_wrong_annotations;
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
	if (defined $self->{extrainfo} && $self->{extrainfo}{subdir} ne '') {
		return $self->{extrainfo}{subdir};
	} else {
		return undef;
	}
}

sub fullpkgpath2
{
	my $self = shift;
	if (defined $self->{extrainfo} && $self->{extrainfo}{subdir} ne '') {
		return $self->{extrainfo}{path};
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
		my $f = $self->fullpkgpath2;
		if (defined $f) {
			push(@{$h->{$f->{dir}}}, $f);
		}
		if (defined $self->{pkgpath}) {
			for my $i (@{$self->{pkgpath}}) {
				push(@{$h->{$i->{path}{dir}}}, $i->{path});
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
    (qw(name url signer digital-signature no-default-conflict manual-installation firmware always-update extrainfo localbase arch));

our @list_categories =
    (qw(conflict pkgpath ask-update depend
    	wantlib define-tag groups users items));

our @cache_categories =
    (qw(depend wantlib));

sub visit
{
	my ($self, $method, @l) = @_;

	if (defined $self->{cvstags}) {
		for my $item (@{$self->{cvstags}}) {
			$item->$method(@l) unless $item->{deleted};
		}
	}

	# XXX unique and info files really get deleted, so there's no need
	# to remove them later.
	for my $unique_item (@unique_categories) {
		$self->{$unique_item}->$method(@l) 
		    if defined $self->{$unique_item};
	}

	for my $special (OpenBSD::PackageInfo::info_names()) {
		$self->{$special}->$method(@l) if defined $self->{$special};
	}

	for my $listname (@list_categories) {
		if (defined $self->{$listname}) {
			for my $item (@{$self->{$listname}}) {
				$item->$method(@l) if !$item->{deleted};
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
	my $plist = OpenBSD::PackingList::Depend->new;
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

sub check_signature
{
	my ($plist, $state) = @_;
	my $sig = $plist->get('digital-signature');
	if ($sig->{key} eq 'x509') {
		require OpenBSD::x509;
		return OpenBSD::x509::check_signature($plist, $state);
	} elsif ($sig->{key} eq 'signify') {
		require OpenBSD::signify;
		return OpenBSD::signify::check_signature($plist, $state);
	} else {
		$state->log("Error: unknown signature style $sig->{key}");
		return 0;
	}
}

sub forget
{
}

sub signature
{
	my $self = shift;

	require OpenBSD::Signature;
	return OpenBSD::Signature->from_plist($self);
}

$subclass =  {
	\&defaultCode => 'Full',
	\&SharedItemsOnly => 'SharedItems',
	\&DirrmOnly => 'SharedItems',
	\&LibraryOnly => 'Libraries',
	\&FilesOnly => 'Files',
	\&PrelinkStuffOnly => 'Prelink',
	\&DependOnly => 'Depend',
	\&ExtraInfoOnly => 'ExtraInfo',
	\&UpdateInfoOnly => 'UpdateInfo',
	\&ConflictOnly => 'Conflict' };

package OpenBSD::PackingList::OldLibs;
our @ISA = qw(OpenBSD::PackingList);
package OpenBSD::PackingList::Full;
our @ISA = qw(OpenBSD::PackingList::OldLibs);
package OpenBSD::PackingList::SharedItems;
our @ISA = qw(OpenBSD::PackingList);
package OpenBSD::PackingList::Libraries;
our @ISA = qw(OpenBSD::PackingList);
package OpenBSD::PackingList::Files;
our @ISA = qw(OpenBSD::PackingList);
package OpenBSD::PackingList::Prelink;
our @ISA = qw(OpenBSD::PackingList);
package OpenBSD::PackingList::Depend;
our @ISA = qw(OpenBSD::PackingList);
package OpenBSD::PackingList::ExtraInfo;
our @ISA = qw(OpenBSD::PackingList);
package OpenBSD::PackingList::UpdateInfo;
our @ISA = qw(OpenBSD::PackingList);
package OpenBSD::PackingList::Conflict;
our @ISA = qw(OpenBSD::PackingList);

1;
