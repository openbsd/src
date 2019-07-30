# ex:ts=8 sw=4:
# $OpenBSD: PackingElement.pm,v 1.250 2018/02/06 15:17:26 espie Exp $
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

use OpenBSD::PackageInfo;
use OpenBSD::Paths;

# perl ipc
require 5.008_000;

# This is the basic class, which is mostly abstract, except for
# create and register_with_factory.
# It does provide base methods for stuff under it, though.
package OpenBSD::PackingElement;
our %keyword;

sub create
{
	my ($class, $line, $plist) = @_;
	if ($line =~ m/^\@(\S+)\s*(.*)$/o) {
		if (defined $keyword{$1}) {
			$keyword{$1}->add($plist, $2);
		} else {
			die "Unknown element: $line";
		}
	} else {
		chomp $line;
		OpenBSD::PackingElement::File->add($plist, $line);
	}
}

sub register_with_factory
{
	my ($class, $k, $o) = @_;
	if (!defined $k) {
		$k = $class->keyword;
	}
	if (!defined $o) {
		$o = $class;
	}
	$keyword{$k} = $o;
}

sub category() { 'items' }

sub new
{
	my ($class, $args) = @_;
	bless { name => $args }, $class;
}

sub remove
{
	my ($self, $plist) = @_;
	$self->{deleted} = 1;
}

sub clone
{
	my $object = shift;
	# shallow copy
	my %h = %$object;
	bless \%h, ref($object);
}


sub register_manpage
{
}

sub destate
{
}

sub add_object
{
	my ($self, $plist) = @_;
	$self->destate($plist->{state});
	$plist->add2list($self);
	return $self;
}

sub add
{
	my ($class, $plist, @args) = @_;

	my $self = $class->new(@args);
	return $self->add_object($plist);
}

sub needs_keyword() { 1 }

sub write
{
	my ($self, $fh) = @_;
	my $s = $self->stringize;
	if ($self->needs_keyword) {
		$s = " $s" unless $s eq '';
		print $fh "\@", $self->keyword, "$s\n";
	} else {
		print $fh "$s\n";
	}
}

sub write_no_sig
{
	my ($self, $fh) = @_;
	$self->write($fh);
}

sub write_without_variation
{
	my ($self, $fh) = @_;
	$self->write_no_sig($fh);
}

# needed for comment checking
sub fullstring
{
	my ($self, $fh) = @_;
	my $s = $self->stringize;
	if ($self->needs_keyword) {
		$s = " $s" unless $s eq '';
		return "\@".$self->keyword.$s;
	} else {
		return $s;
	}
}

sub name
{
	my $self = shift;
	return $self->{name};
}

sub set_name
{
	my ($self, $v) = @_;
	$self->{name} = $v;
}
sub stringize
{
	my $self = shift;
	return $self->name;
}

sub IsFile() { 0 }

sub is_a_library() { 0 }
sub NoDuplicateNames() { 0 }


sub copy_shallow_if
{
	my ($self, $copy, $h) = @_;
	$self->add_object($copy) if defined $h->{$self};
}

sub copy_deep_if
{
	my ($self, $copy, $h) = @_;
	$self->clone->add_object($copy) if defined $h->{$self};
}

sub finish
{
	my ($class, $state) = @_;
	OpenBSD::PackingElement::Fontdir->finish($state);
	OpenBSD::PackingElement::RcScript->report($state);
	if ($state->{readmes}) {
		$state->say("Look in #1/share/doc/pkg-readmes for extra documentation.", $state->{localbase});
	}
}

# Basic class hierarchy

# various stuff that's only linked to objects before/after them
# this class doesn't have real objects: no valid new nor clone...
package OpenBSD::PackingElement::Annotation;
our @ISA=qw(OpenBSD::PackingElement);
sub new { die "Can't create annotation objects" }

# concrete objects
package OpenBSD::PackingElement::Object;
our @ISA=qw(OpenBSD::PackingElement);

sub cwd
{
	return ${$_[0]->{cwd}};
}

sub absolute_okay() { 0 }
sub compute_fullname
{
	my ($self, $state) = @_;

	$self->{cwd} = $state->{cwd};
	$self->set_name(File::Spec->canonpath($self->name));
	if ($self->name =~ m|^/|) {
		unless ($self->absolute_okay) {
			die "Absolute name forbidden: ", $self->name;
		}
	}
}

sub make_full
{
	my ($self, $path) = @_;
	if ($path !~ m|^/|o && $self->cwd ne '.') {
		$path = $self->cwd."/".$path;
		$path =~ s,^//,/,;
	}
	return $path;
}

sub fullname
{
	my $self = shift;
	return $self->make_full($self->name);
}

sub compute_modes
{
	my ($self, $state) = @_;
	if (defined $state->{mode}) {
		$self->{mode} = $state->{mode};
	}
	if (defined $state->{owner}) {
		$self->{owner} = $state->{owner};
		if (defined $state->{uid}) {
			$self->{uid} = $state->{uid};
		}
	}
	if (defined $state->{group}) {
		$self->{group} = $state->{group};
		if (defined $state->{gid}) {
			$self->{gid} = $state->{gid};
		}
	}
}

# concrete objects with file-like behavior
package OpenBSD::PackingElement::FileObject;
our @ISA=qw(OpenBSD::PackingElement::Object);

sub NoDuplicateNames() { 1 }

sub dirclass() { undef }

sub new
{
	my ($class, $args) = @_;
	if ($args =~ m/^(.*?)\/+$/o and defined $class->dirclass) {
		bless { name => $1 }, $class->dirclass;
	} else {
		bless { name => $args }, $class;
	}
}

sub destate
{
	my ($self, $state) = @_;
	$state->{lastfileobject} = $self;
	$self->compute_fullname($state);
}

sub set_tempname
{
	my ($self, $tempname) = @_;
	$self->{tempname} = $tempname;
}

sub realname
{
	my ($self, $state) = @_;

	my $name = $self->fullname;
	if (defined $self->{tempname}) {
		$name = $self->{tempname};
	}
	return $state->{destdir}.$name;
}

sub compute_digest
{
	my ($self, $filename, $class) = @_;
	require OpenBSD::md5;
	$class = 'OpenBSD::sha' if !defined $class;
	return $class->new($filename);
}

sub write
{
	my ($self, $fh) = @_;

	$self->SUPER::write($fh);
	if (defined $self->{tags}) {
		for my $tag (sort keys %{$self->{tags}}) {
			print $fh "\@tag ", $tag, "\n";
		}
	}
}

# exec/unexec and friends
package OpenBSD::PackingElement::Action;
our @ISA=qw(OpenBSD::PackingElement::Object);

# persistent state for following objects
package OpenBSD::PackingElement::State;
our @ISA=qw(OpenBSD::PackingElement::Object);

# meta information, stored elsewhere
package OpenBSD::PackingElement::Meta;
our @ISA=qw(OpenBSD::PackingElement);

package OpenBSD::PackingElement::Unique;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub add_object
{
	my ($self, $plist) = @_;

	$self->destate($plist->{state});
	$plist->addunique($self);
	return $self;
}

sub remove
{
	my ($self, $plist) = @_;
	delete $plist->{$self->category};
}

sub category
{
	return ref(shift);
}

# all the stuff that ends up in signatures
package OpenBSD::PackingElement::VersionElement;
our @ISA=qw(OpenBSD::PackingElement::Meta);

# all dependency information
package OpenBSD::PackingElement::Depend;
our @ISA=qw(OpenBSD::PackingElement::VersionElement);

# Abstract class for all file-like elements
package OpenBSD::PackingElement::FileBase;
our @ISA=qw(OpenBSD::PackingElement::FileObject);

use File::Basename;

sub write
{
	my ($self, $fh) = @_;
	print $fh "\@comment no checksum\n" if defined $self->{nochecksum};
	$self->SUPER::write($fh);
	if (defined $self->{d}) {
		$self->{d}->write($fh);
	}
	if (defined $self->{size}) {
		print $fh "\@size ", $self->{size}, "\n";
	}
	if (defined $self->{ts}) {
		print $fh "\@ts ", $self->{ts}, "\n";
	}
	if (defined $self->{symlink}) {
		print $fh "\@symlink ", $self->{symlink}, "\n";
	}
	if (defined $self->{link}) {
		print $fh "\@link ", $self->{link}, "\n";
	}
	if (defined $self->{tempname}) {
		print $fh "\@temp ", $self->{tempname}, "\n";
	}
}

sub destate
{
	my ($self, $state) = @_;
	$self->SUPER::destate($state);
	$state->{lastfile} = $self;
	$state->{lastchecksummable} = $self;
	$self->compute_modes($state);
	if (defined $state->{nochecksum}) {
		$self->{nochecksum} = 1;
		undef $state->{nochecksum};
	}
}

sub add_digest
{
	my ($self, $d) = @_;
	$self->{d} = $d;
}
sub add_size
{
	my ($self, $sz) = @_;
	$self->{size} = $sz;
}

sub add_timestamp
{
	my ($self, $ts) = @_;
	$self->{ts} = $ts;
}

# XXX symlink/hardlinks are properties of File,
# because we want to use inheritance for other stuff.

sub make_symlink
{
	my ($self, $linkname) = @_;
	$self->{symlink} = $linkname;
}

sub make_hardlink
{
	my ($self, $linkname) = @_;
	$self->{link} = $linkname;
}

sub may_check_digest
{
	my ($self, $file, $state) = @_;
	if ($state->{check_digest}) {
		$self->check_digest($file, $state);
	}
}

sub check_digest
{
	my ($self, $file, $state) = @_;
	return if $self->{link} or $self->{symlink};
	if (!defined $self->{d}) {
		$state->log->fatal($state->f("#1 does not have a signature",
		    $self->fullname));
	}
	my $d = $self->compute_digest($file->{destdir}.$file->name);
	if (!$d->equals($self->{d})) {
		$state->log->fatal($state->f("checksum for #1 does not match",
		    $self->fullname));
	}
	if ($state->verbose >= 3) {
		$state->say("Checksum match for #1", $self->fullname);
	}
}

sub IsFile() { 1 }

package OpenBSD::PackingElement::File;
our @ISA=qw(OpenBSD::PackingElement::FileBase);

use OpenBSD::PackageInfo qw(is_info_name);
sub keyword() { "file" }
__PACKAGE__->register_with_factory;

sub dirclass() { "OpenBSD::PackingElement::Dir" }

sub needs_keyword
{
	my $self = shift;
	return $self->stringize =~ m/\^@/;
}

sub add_object
{
	my ($self, $plist) = @_;

	$self->destate($plist->{state});
	my $j = is_info_name($self->fullname);
	if ($j) {
		bless $self, "OpenBSD::PackingElement::$j";
		$self->add_object($plist);
	} else {
		$plist->add2list($self);
	}
	return $self;
}

package OpenBSD::PackingElement::Sample;
our @ISA=qw(OpenBSD::PackingElement::FileObject);

sub keyword() { "sample" }
sub absolute_okay() { 1 }
__PACKAGE__->register_with_factory;

sub destate
{
	my ($self, $state) = @_;
	if ($state->{lastfile}->isa("OpenBSD::PackingElement::SpecialFile")) {
		die "Can't \@sample a specialfile: ". 
		    $state->{lastfile}->stringize. "\n";
	}
	$self->{copyfrom} = $state->{lastfile};
	$self->compute_fullname($state);
	$self->compute_modes($state);
}

sub dirclass() { "OpenBSD::PackingElement::Sampledir" }

package OpenBSD::PackingElement::Ghost;
our @ISA = qw(OpenBSD::PackingElement::FileObject);

sub keyword() { "ghost" }
sub absolute_okay() { 1 }
__PACKAGE__->register_with_factory;

sub destate
{
	my ($self, $state) = @_;
	$self->compute_fullname($state);
	$self->compute_modes($state);
}

package OpenBSD::PackingElement::Sampledir;
our @ISA=qw(OpenBSD::PackingElement::DirBase OpenBSD::PackingElement::Sample);

sub absolute_okay() { 1 }

sub destate
{
	my ($self, $state) = @_;
	$self->compute_fullname($state);
	$self->compute_modes($state);
}

package OpenBSD::PackingElement::RcScript;
use File::Basename;
our @ISA = qw(OpenBSD::PackingElement::FileBase);

sub keyword() { "rcscript" }
sub absolute_okay() { 1 }
__PACKAGE__->register_with_factory;

sub destate
{
	my ($self, $state) = @_;
	$self->compute_fullname($state);
	if ($self->name =~ m/^\//) {
		$state->set_cwd(dirname($self->name));
	}
	$state->{lastfile} = $self;
	$state->{lastchecksummable} = $self;
	$self->compute_modes($state);
}

sub report
{
	my ($class, $state) = @_;

	my @l;
	for my $script (sort keys %{$state->{add_rcscripts}}) {
		next if $state->{delete_rcscripts}{$script};
		push(@l, $script);
	}
	if (@l > 0) {
		$state->say("The following new rcscripts were installed: #1",
		    join(' ', @l));
		$state->say("See rcctl(8) for details.");
	}
}

package OpenBSD::PackingElement::InfoFile;
our @ISA=qw(OpenBSD::PackingElement::FileBase);

sub keyword() { "info" }
__PACKAGE__->register_with_factory;
sub dirclass() { "OpenBSD::PackingElement::Infodir" }

package OpenBSD::PackingElement::Shell;
our @ISA=qw(OpenBSD::PackingElement::FileBase);

sub keyword() { "shell" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::Manpage;
use File::Basename;
our @ISA=qw(OpenBSD::PackingElement::FileBase);

sub keyword() { "man" }
__PACKAGE__->register_with_factory;

sub register_manpage
{
	my ($self, $state, $key) = @_;
	# XXX don't bother register stuff from partial packages
	return if defined $self->{tempname};
	my $fname = $self->fullname;
	if ($fname =~ m,^(.*/man(?:/\w+)?)/((?:man|cat)[1-9n]\w*/.*),) {
		push(@{$state->{$key}{$1}}, $2);
    	}
}

sub is_source
{
	my $self = shift;
	return $self->name =~ m/man\/man[^\/]+\/[^\/]+\.[\dln][^\/]?$/o;
}

sub source_to_dest
{
	my $self = shift;
	my $v = $self->name;
	$v =~ s/(man\/)man([^\/]+\/[^\/]+)\.[\dln][^\/]?$/$1cat$2.0/;
	return $v;
}

# assumes the source is nroff, launches nroff
sub format
{
	my ($self, $state, $dest, $destfh) = @_;

	my $base = $state->{base};
	my $fname = $base.$self->fullname;
	if (-z $fname) {
		$state->error("empty source manpage: #1", $fname);
		return;
	}
	open(my $fh, '<', $fname) or die "Can't read $fname";
	my $line = <$fh>;
	close $fh;
	my @extra = ();
	# extra preprocessors as described in man.
	if ($line =~ m/^\'\\\"\s+(.*)$/o) {
		for my $letter (split '', $1) {
			if ($letter =~ m/[ept]/o) {
				push(@extra, "-$letter");
			} elsif ($letter eq 'r') {
				push(@extra, "-R");
			}
		}
	}
	my $d = dirname($dest);
	unless (-d $d) {
		mkdir($d);
	}
	if (my ($dir, $file) = $fname =~ m/^(.*)\/([^\/]+\/[^\/]+)$/) {
		$state->system(sub {
		    open STDOUT, '>&', $destfh or
			die "Can't write to $dest";
		    close $destfh;
		    chdir($dir) or die "Can't chdir to $dir";
		    },
		    OpenBSD::Paths->groff,
		    qw(-mandoc -mtty-char -E -Ww -Tascii -P -c),
		    @extra, '--', $file);
	} else {
		die "Can't parse source name $fname";
	}
	return 1;
}

package OpenBSD::PackingElement::Mandoc;
our @ISA=qw(OpenBSD::PackingElement::Manpage);

sub keyword() { "mandoc" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::Lib;
our @ISA=qw(OpenBSD::PackingElement::FileBase);

our $todo = 0;

sub keyword() { "lib" }
__PACKAGE__->register_with_factory;

sub mark_ldconfig_directory
{
	my ($self, $state) = @_;
	$state->ldconfig->mark_directory($self->fullname);
}

sub parse
{
	my ($self, $filename) = @_;
	if ($filename =~ m/^(.*?)\/?lib([^\/]+)\.so\.(\d+)\.(\d+)$/o) {
		return ($2, $3, $4, $1);
	} else {
		return undef;
	}
}

sub is_a_library() { 1 }

package OpenBSD::PackingElement::PkgConfig;
our @ISA=qw(OpenBSD::PackingElement::FileBase);

sub keyword() { "pkgconfig" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::LibtoolLib;
our @ISA=qw(OpenBSD::PackingElement::FileBase);

sub keyword() { "ltlib" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::Binary;
our @ISA=qw(OpenBSD::PackingElement::FileBase);

sub keyword() { "bin" }
__PACKAGE__->register_with_factory;

# Comment is very special
package OpenBSD::PackingElement::Comment;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub keyword() { "comment" }
__PACKAGE__->register_with_factory;

sub destate
{
	my ($self, $state) = @_;
	$self->{cwd} = $state->{cwd};
}

sub add
{
	my ($class, $plist, $args) = @_;

	if ($args =~ m/^\$OpenBSD.*\$\s*$/o) {
		return OpenBSD::PackingElement::CVSTag->add($plist, $args);
	} elsif ($args =~ m/^(?:subdir|pkgpath)\=(.*?)\s+cdrom\=(.*?)\s+ftp\=(.*?)\s*$/o) {
		return OpenBSD::PackingElement::ExtraInfo->add($plist, $1, $2, $3);
	} elsif ($args eq 'no checksum') {
		$plist->{state}->{nochecksum} = 1;
		return;
	} else {
		return $class->SUPER::add($plist, $args);
	}
}

package OpenBSD::PackingElement::CVSTag;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub keyword() { 'comment' }

sub category() { 'cvstags'}

# don't incorporate this into compared signatures
sub write_without_variation
{
}

package OpenBSD::PackingElement::sha;
our @ISA=qw(OpenBSD::PackingElement::Annotation);

__PACKAGE__->register_with_factory('sha');

sub add
{
	my ($class, $plist, $args) = @_;

	require OpenBSD::md5;

	$plist->{state}->{lastchecksummable}->add_digest(OpenBSD::sha->fromstring($args));
	return;
}

package OpenBSD::PackingElement::tag;
our @ISA=qw(OpenBSD::PackingElement::Annotation);

__PACKAGE__->register_with_factory('tag');

sub add
{
	my ($class, $plist, $args) = @_;

	if ($args eq 'no checksum') {
		$plist->{state}{lastfile}{nochecksum} = 1;
	} else {
		my $object = $plist->{state}{lastfileobject};
		$object->{tags}{$args} = 1;
		push(@{$plist->{tags}{$args}}, $object);
	}
	return undef;
}

package OpenBSD::PackingElement::DefineTag;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub category() { 'define-tag' }
sub keyword() { 'define-tag' }
__PACKAGE__->register_with_factory;

sub new
{
	my ($class, $args) = @_;
	my ($tag, $condition, @command) = split(/\s+/, $args);
	bless {
		name => $tag,
		when => $condition,
		command => join(' ', @command)
	}, $class;
}

sub stringize
{
	my $self = shift;
	return join(' ', map { $self->{$_}}
		(qw(name when command)));
}

package OpenBSD::PackingElement::symlink;
our @ISA=qw(OpenBSD::PackingElement::Annotation);

__PACKAGE__->register_with_factory('symlink');

sub add
{
	my ($class, $plist, $args) = @_;

	$plist->{state}->{lastfile}->make_symlink($args);
	return;
}

package OpenBSD::PackingElement::hardlink;
our @ISA=qw(OpenBSD::PackingElement::Annotation);

__PACKAGE__->register_with_factory('link');

sub add
{
	my ($class, $plist, $args) = @_;

	$plist->{state}->{lastfile}->make_hardlink($args);
	return;
}

package OpenBSD::PackingElement::temp;
our @ISA=qw(OpenBSD::PackingElement::Annotation);

__PACKAGE__->register_with_factory('temp');

sub add
{
	my ($class, $plist, $args) = @_;
	$plist->{state}->{lastfile}->set_tempname($args);
	return;
}

package OpenBSD::PackingElement::size;
our @ISA=qw(OpenBSD::PackingElement::Annotation);

__PACKAGE__->register_with_factory('size');

sub add
{
	my ($class, $plist, $args) = @_;

	$plist->{state}->{lastfile}->add_size($args);
	return;
}

package OpenBSD::PackingElement::ts;
our @ISA=qw(OpenBSD::PackingElement::Annotation);

__PACKAGE__->register_with_factory('ts');

sub add
{
	my ($class, $plist, $args) = @_;

	$plist->{state}->{lastfile}->add_timestamp($args);
	return;
}

package OpenBSD::PackingElement::Option;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub keyword() { 'option' }
__PACKAGE__->register_with_factory;

sub new
{
	my ($class, $args) = @_;
	if ($args eq 'no-default-conflict') {
		return OpenBSD::PackingElement::NoDefaultConflict->new;
	} elsif ($args eq 'manual-installation') {
		return OpenBSD::PackingElement::ManualInstallation->new;
	} elsif ($args eq 'firmware') {
		return OpenBSD::PackingElement::Firmware->new;
	} elsif ($args eq 'always-update') {
		return OpenBSD::PackingElement::AlwaysUpdate->new;
	} elsif ($args eq 'is-branch') {
		return OpenBSD::PackingElement::IsBranch->new;
	} else {
		die "Unknown option: $args";
	}
}

package OpenBSD::PackingElement::UniqueOption;
our @ISA=qw(OpenBSD::PackingElement::Unique OpenBSD::PackingElement::Option);

sub stringize
{
	my $self = shift;
	return $self->category;
}

sub new
{
	my ($class, @args) = @_;
	bless {}, $class;
}

package OpenBSD::PackingElement::NoDefaultConflict;
our @ISA=qw(OpenBSD::PackingElement::UniqueOption);

sub category() { 'no-default-conflict' }

package OpenBSD::PackingElement::ManualInstallation;
our @ISA=qw(OpenBSD::PackingElement::UniqueOption);

sub category() { 'manual-installation' }

# XXX don't incorporate this in signatures.
sub write_no_sig()
{
}

package OpenBSD::PackingElement::Firmware;
our @ISA=qw(OpenBSD::PackingElement::ManualInstallation);
sub category() { 'firmware' }

package OpenBSD::PackingElement::AlwaysUpdate;
our @ISA=qw(OpenBSD::PackingElement::UniqueOption);

sub category()
{
	'always-update';
}

package OpenBSD::PackingElement::IsBranch;
our @ISA=qw(OpenBSD::PackingElement::UniqueOption);

sub category()
{
	'is-branch';
}
# The special elements that don't end in the right place
package OpenBSD::PackingElement::ExtraInfo;
our @ISA=qw(OpenBSD::PackingElement::Unique OpenBSD::PackingElement::Comment);

sub category() { 'extrainfo' }

sub new
{
	my ($class, $subdir, $cdrom, $ftp) = @_;

	$cdrom =~ s/^\"(.*)\"$/$1/;
	$cdrom =~ s/^\'(.*)\'$/$1/;
	$ftp =~ s/^\"(.*)\"$/$1/;
	$ftp =~ s/^\'(.*)\'$/$1/;
	bless { subdir => $subdir,
		path => OpenBSD::PkgPath->new($subdir),
	    cdrom => $cdrom,
	    ftp => $ftp}, $class;
}

sub subdir
{
	return shift->{subdir};
}

sub may_quote
{
	my $s = shift;
	if ($s =~ m/\s/) {
		return '"'.$s.'"';
	} else {
		return $s;
	}
}

sub stringize
{
	my $self = shift;
	return join(' ',
	    "pkgpath=".$self->{subdir},
	    "cdrom=".may_quote($self->{cdrom}),
	    "ftp=".may_quote($self->{ftp}));
}

package OpenBSD::PackingElement::Name;
use File::Spec;
our @ISA=qw(OpenBSD::PackingElement::Unique);

sub keyword() { "name" }
__PACKAGE__->register_with_factory;
sub category() { "name" }

package OpenBSD::PackingElement::LocalBase;
our @ISA=qw(OpenBSD::PackingElement::Unique);

sub keyword() { "localbase" }
__PACKAGE__->register_with_factory;
sub category() { "localbase" }

package OpenBSD::PackingElement::Url;
our @ISA=qw(OpenBSD::PackingElement::Unique);

sub keyword() { "url" }
__PACKAGE__->register_with_factory;
sub category() { "url" }

# XXX don't incorporate this in signatures.
sub write_no_sig()
{
}

package OpenBSD::PackingElement::Version;
our @ISA=qw(OpenBSD::PackingElement::Unique OpenBSD::PackingElement::VersionElement);

sub keyword() { "version" }
__PACKAGE__->register_with_factory;
sub category() { "version" }

package OpenBSD::PackingElement::Conflict;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub keyword() { "conflict" }
__PACKAGE__->register_with_factory;
sub category() { "conflict" }

sub spec
{
	my $self =shift;

	require OpenBSD::Search;
	return OpenBSD::Search::PkgSpec->new($self->name);
}

package OpenBSD::PackingElement::Dependency;
our @ISA=qw(OpenBSD::PackingElement::Depend);
use OpenBSD::Error;

sub keyword() { "depend" }
__PACKAGE__->register_with_factory;
sub category() { "depend" }

sub new
{
	my ($class, $args) = @_;
	my ($pkgpath, $pattern, $def) = split /\:/o, $args;
	bless { name => $def, pkgpath => $pkgpath, pattern => $pattern,
	    def => $def }, $class;
}

sub stringize
{
	my $self = shift;
	return join(':', map { $self->{$_}}
	    (qw(pkgpath pattern def)));
}

OpenBSD::Auto::cache(spec,
    sub {
	require OpenBSD::Search;

	my $self = shift;
	return OpenBSD::Search::PkgSpec->new($self->{pattern})
	    ->add_pkgpath_hint($self->{pkgpath});
    });

package OpenBSD::PackingElement::Wantlib;
our @ISA=qw(OpenBSD::PackingElement::Depend);

sub category() { "wantlib" }
sub keyword() { "wantlib" }
__PACKAGE__->register_with_factory;

OpenBSD::Auto::cache(spec,
    sub {
    	my $self = shift;

    	require OpenBSD::LibSpec;
	return OpenBSD::LibSpec->from_string($self->name);
    });

package OpeNBSD::PackingElement::Libset;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub category() { "libset" }
sub keyword() { "libset" }
__PACKAGE__->register_with_factory;

sub new
{
	my ($class, $args) = @_;
	if ($args =~ m/(.*)\:(.*)/) {
		return bless {name => $1, libs => [split(/\,/, $2)]}, $class;
	} else {
		die "Bad args for libset: $args";
	}
}

sub stringize
{
	my $self = shift;
	return $self->{name}.':'.join(',', @{$self->{libs}});
}

package OpenBSD::PackingElement::PkgPath;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub keyword() { "pkgpath" }
__PACKAGE__->register_with_factory;
sub category() { "pkgpath" }

sub new
{
	my ($class, $fullpkgpath) = @_;
	bless {name => $fullpkgpath,
	    path => OpenBSD::PkgPath::WithOpts->new($fullpkgpath)}, $class;
}

sub subdir
{
	return shift->{name};
}

package OpenBSD::PackingElement::AskUpdate;
our @ISA=qw(OpenBSD::PackingElement::Meta);

sub new
{
	my ($class, $args) = @_;
	my ($pattern, $message) = split /\s+/o, $args, 2;
	bless { pattern => $pattern, message => $message}, $class;
}

sub stringize
{
	my $self = shift;
	return join(' ', map { $self->{$_}}
	    (qw(pattern message)));
}

sub keyword() { "ask-update" }
__PACKAGE__->register_with_factory;
sub category() { "ask-update" }

OpenBSD::Auto::cache(spec,
    sub {
	require OpenBSD::PkgSpec;

	my $self = shift;
	return OpenBSD::PkgSpec->new($self->{pattern})
    });

package OpenBSD::PackingElement::NewAuth;
our @ISA=qw(OpenBSD::PackingElement::Action);

package OpenBSD::PackingElement::NewUser;
our @ISA=qw(OpenBSD::PackingElement::NewAuth);

sub type() { "user" }
sub category() { "users" }
sub keyword() { "newuser" }
__PACKAGE__->register_with_factory;

sub new
{
	my ($class, $args) = @_;
	my ($name, $uid, $group, $loginclass, $comment, $home, $shell) =
	    split /\:/o, $args;
	bless { name => $name, uid => $uid, group => $group,
	    class => $loginclass,
	    comment => $comment, home => $home, shell => $shell }, $class;
}

sub destate
{
	my ($self, $state) = @_;
	my $uid = $self->{uid};
	$uid =~ s/^\!//;
	$state->{owners}{$self->{name}} = $uid;
}

sub check
{
	my $self = shift;
	my ($name, $passwd, $uid, $gid, $quota, $class, $gcos, $dir, $shell,
	    $expire) = getpwnam($self->name);
	return unless defined $name;
	if ($self->{uid} =~ m/^\!(.*)$/o) {
		return 0 unless $uid == $1;
	}
	if ($self->{group} =~ m/^\!(.*)$/o) {
		my $g = $1;
		unless ($g =~ m/^\d+$/o) {
			$g = getgrnam($g);
			return 0 unless defined $g;
		}
		return 0 unless $gid eq $g;
	}
	if ($self->{class} =~ m/^\!(.*)$/o) {
		return 0 unless $class eq $1;
	}
	if ($self->{comment} =~ m/^\!(.*)$/o) {
		return 0 unless $gcos eq $1;
	}
	if ($self->{home} =~ m/^\!(.*)$/o) {
		return 0 unless $dir eq $1;
	}
	if ($self->{shell} =~ m/^\!(.*)$/o) {
		return 0 unless $shell eq $1;
	}
	return 1;
}

sub stringize
{
	my $self = shift;
	return join(':', map { $self->{$_}}
	    (qw(name uid group class comment home shell)));
}

package OpenBSD::PackingElement::NewGroup;
our @ISA=qw(OpenBSD::PackingElement::NewAuth);


sub type() { "group" }
sub category() { "groups" }
sub keyword() { "newgroup" }
__PACKAGE__->register_with_factory;

sub new
{
	my ($class, $args) = @_;
	my ($name, $gid) = split /\:/o, $args;
	bless { name => $name, gid => $gid }, $class;
}

sub destate
{
	my ($self, $state) = @_;
	my $gid = $self->{gid};
	$gid =~ s/^\!//;
	$state->{groups}{$self->{name}} = $gid;
}

sub check
{
	my $self = shift;
	my ($name, $passwd, $gid, $members) = getgrnam($self->name);
	return unless defined $name;
	if ($self->{gid} =~ m/^\!(.*)$/o) {
		return 0 unless $gid == $1;
	}
	return 1;
}

sub stringize($)
{
	my $self = $_[0];
	return join(':', map { $self->{$_}}
	    (qw(name gid)));
}

package OpenBSD::PackingElement::Cwd;
use File::Spec;
our @ISA=qw(OpenBSD::PackingElement::State);


sub keyword() { 'cwd' }
__PACKAGE__->register_with_factory;

sub destate
{
	my ($self, $state) = @_;
	$state->set_cwd($self->name);
}

package OpenBSD::PackingElement::Owner;
our @ISA=qw(OpenBSD::PackingElement::State);

sub keyword() { 'owner' }
__PACKAGE__->register_with_factory;

sub destate
{
	my ($self, $state) = @_;

	delete $state->{uid};
	if ($self->name eq '') {
		undef $state->{owner};
	} else {
		$state->{owner} = $self->name;
		if (defined $state->{owners}{$self->name}) {
			$state->{uid} = $state->{owners}{$self->name};
		}
	}
}

package OpenBSD::PackingElement::Group;
our @ISA=qw(OpenBSD::PackingElement::State);

sub keyword() { 'group' }
__PACKAGE__->register_with_factory;

sub destate
{
	my ($self, $state) = @_;

	delete $state->{gid};
	if ($self->name eq '') {
		undef $state->{group};
	} else {
		$state->{group} = $self->name;
		if (defined $state->{groups}{$self->name}) {
			$state->{gid} = $state->{groups}{$self->name};
		}
	}
}

package OpenBSD::PackingElement::Mode;
our @ISA=qw(OpenBSD::PackingElement::State);

sub keyword() { 'mode' }
__PACKAGE__->register_with_factory;

sub destate
{
	my ($self, $state) = @_;

	if ($self->name eq '') {
		undef $state->{mode};
	} else {
		$state->{mode} = $self->name;
	}
}

package OpenBSD::PackingElement::ExeclikeAction;
use File::Basename;
use OpenBSD::Error;
our @ISA=qw(OpenBSD::PackingElement::Action);

sub expand
{
	my ($self, $state) = @_;
	my $e = $self->name;
	if ($e =~ m/\%F/o) {
		die "Bad expand" unless defined $state->{lastfile};
		$e =~ s/\%F/$state->{lastfile}->{name}/g;
	}
	if ($e =~ m/\%D/o) {
		die "Bad expand" unless defined $state->{cwd};
		$e =~ s/\%D/$state->cwd/ge;
	}
	if ($e =~ m/\%B/o) {
		die "Bad expand" unless defined $state->{lastfile};
		$e =~ s/\%B/dirname($state->{lastfile}->fullname)/ge;
	}
	if ($e =~ m/\%f/o) {
		die "Bad expand" unless defined $state->{lastfile};
		$e =~ s/\%f/basename($state->{lastfile}->fullname)/ge;
	}
	return $e;
}

sub destate
{
	my ($self, $state) = @_;
	$self->{expanded} = $self->expand($state);
}

sub run
{
	my ($self, $state) = @_;

	$state->ldconfig->ensure;
	$state->say("#1 #2", $self->keyword, $self->{expanded})
	    if $state->verbose >= 2;
	$state->log->system(OpenBSD::Paths->sh, '-c', $self->{expanded})
	    unless $state->{not};
}

package OpenBSD::PackingElement::Exec;
our @ISA=qw(OpenBSD::PackingElement::ExeclikeAction);

sub keyword() { "exec" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::ExecAlways;
our @ISA=qw(OpenBSD::PackingElement::Exec);

sub keyword() { "exec-always" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::ExecAdd;
our @ISA=qw(OpenBSD::PackingElement::Exec);

sub keyword() { "exec-add" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::ExecUpdate;
our @ISA=qw(OpenBSD::PackingElement::Exec);

sub keyword() { "exec-update" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::Unexec;
our @ISA=qw(OpenBSD::PackingElement::ExeclikeAction);

sub keyword() { "unexec" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::UnexecAlways;
our @ISA=qw(OpenBSD::PackingElement::Unexec);

sub keyword() { "unexec-always" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::UnexecUpdate;
our @ISA=qw(OpenBSD::PackingElement::Unexec);

sub keyword() { "unexec-update" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::UnexecDelete;
our @ISA=qw(OpenBSD::PackingElement::Unexec);

sub keyword() { "unexec-delete" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::ExtraUnexec;
our @ISA=qw(OpenBSD::PackingElement::ExeclikeAction);

sub keyword() { "extraunexec" }
__PACKAGE__->register_with_factory;

package OpenBSD::PackingElement::DirlikeObject;
our @ISA=qw(OpenBSD::PackingElement::FileObject);

package OpenBSD::PackingElement::DirBase;
our @ISA=qw(OpenBSD::PackingElement::DirlikeObject);

sub destate
{
	my ($self, $state) = @_;
	$state->{lastdir} = $self;
	$self->SUPER::destate($state);
}


sub stringize
{
	my $self = shift;
	return $self->name."/";
}

sub write
{
	my ($self, $fh) = @_;
	$self->SUPER::write($fh);
}

package OpenBSD::PackingElement::Dir;
our @ISA=qw(OpenBSD::PackingElement::DirBase);

sub keyword() { "dir" }
__PACKAGE__->register_with_factory;

sub destate
{
	my ($self, $state) = @_;
	$self->SUPER::destate($state);
	$self->compute_modes($state);
}

sub needs_keyword
{
	my $self = shift;
	return $self->stringize =~ m/\^@/o;
}

package OpenBSD::PackingElement::Infodir;
our @ISA=qw(OpenBSD::PackingElement::Dir);
sub keyword() { "info" }
sub needs_keyword() { 1 }

package OpenBSD::PackingElement::Fontdir;
our @ISA=qw(OpenBSD::PackingElement::Dir);
sub keyword() { "fontdir" }
__PACKAGE__->register_with_factory;
sub needs_keyword() { 1 }
sub dirclass() { "OpenBSD::PackingElement::Fontdir" }

sub install
{
	my ($self, $state) = @_;
	$self->SUPER::install($state);
	$state->log("You may wish to update your font path for #1", $self->fullname);
	$state->{recorder}{fonts_todo}{$state->{destdir}.$self->fullname} = 1;
}

sub reload
{
	my ($self, $state) = @_;
	$state->{recorder}{fonts_todo}{$state->{destdir}.$self->fullname} = 1;
}

sub update_fontalias
{
	my $dirname = shift;
	my @aliases;

	if (-d "$dirname") {
		for my $alias (glob "$dirname/fonts.alias-*") {
			open my $f ,'<', $alias or next;
			push(@aliases, <$f>);
			close $f;
		}
		open my $f, '>', "$dirname/fonts.alias";
		print $f @aliases;
		close $f;
	}
}

sub restore_fontdir
{
	my ($dirname, $state) = @_;
	if (-f "$dirname/fonts.dir.dist") {

		unlink("$dirname/fonts.dir");
		$state->copy_file("$dirname/fonts.dir.dist",
		    "$dirname/fonts.dir");
	}
}

sub run_if_exists
{
	my ($state, $cmd, @l) = @_;

	if (-x $cmd) {
		$state->vsystem($cmd, @l);
	} else {
		$state->errsay("#1 not found", $cmd);
	}
}

sub finish
{
	my ($class, $state) = @_;
	my @l = keys %{$state->{recorder}->{fonts_todo}};

	if (@l != 0) {
		require OpenBSD::Error;

		return if $state->{not};
		map { update_fontalias($_) } @l;
		if (-d "@l") {
			run_if_exists($state, OpenBSD::Paths->mkfontscale, '--', @l);
			run_if_exists($state, OpenBSD::Paths->mkfontdir, '--', @l);
			map { restore_fontdir($_, $state) } @l;
		}

		run_if_exists($state, OpenBSD::Paths->fc_cache, '--', @l);
	}
}


package OpenBSD::PackingElement::Mandir;
our @ISA=qw(OpenBSD::PackingElement::Dir);

sub keyword() { "mandir" }
__PACKAGE__->register_with_factory;
sub needs_keyword() { 1 }
sub dirclass() { "OpenBSD::PackingElement::Mandir" }

package OpenBSD::PackingElement::Extra;
our @ISA=qw(OpenBSD::PackingElement::FileObject);

sub keyword() { 'extra' }
sub absolute_okay() { 1 }
__PACKAGE__->register_with_factory;

sub destate
{
	my ($self, $state) = @_;
	$self->compute_fullname($state);
}

sub dirclass() { "OpenBSD::PackingElement::Extradir" }

package OpenBSD::PackingElement::Extradir;
our @ISA=qw(OpenBSD::PackingElement::DirBase OpenBSD::PackingElement::Extra);
sub absolute_okay() { 1 }

sub destate
{
	&OpenBSD::PackingElement::Extra::destate;
}

package OpenBSD::PackingElement::SpecialFile;
our @ISA=qw(OpenBSD::PackingElement::Unique);

sub add_digest
{
	&OpenBSD::PackingElement::FileBase::add_digest;
}

sub add_size
{
	&OpenBSD::PackingElement::FileBase::add_size;
}

sub add_timestamp
{
	# just don't
}

sub compute_digest
{
	&OpenBSD::PackingElement::FileObject::compute_digest;
}

sub write
{
	&OpenBSD::PackingElement::FileBase::write;
}

sub needs_keyword { 0 }

sub add_object
{
	my ($self, $plist) = @_;
	$self->{infodir} = $plist->{infodir};
	$self->SUPER::add_object($plist);
}

sub infodir
{
	my $self = shift;
	return ${$self->{infodir}};
}

sub stringize
{
	my $self = shift;
	return $self->category;
}

sub fullname
{
	my $self = shift;
	my $d = $self->infodir;
	if (defined $d) {
		return $d.$self->name;
	} else {
		return undef;
	}
}

sub category
{
	my $self = shift;

	return $self->name;
}

sub new
{
	&OpenBSD::PackingElement::UniqueOption::new;
}

sub may_verify_digest
{
	my ($self, $state) = @_;
	if (!$state->{check_digest}) {
		return;
	}
	if (!defined $self->{d}) {
		$state->log->fatal($state->f("#1 does not have a signature",
		    $self->fullname));
	}
	my $d = $self->compute_digest($self->fullname);
	if (!$d->equals($self->{d})) {
		$state->log->fatal($state->f("checksum for #1 does not match",
		    $self->fullname));
	}
	if ($state->verbose >= 3) {
		$state->say("Checksum match for #1", $self->fullname);
	}
}

package OpenBSD::PackingElement::FCONTENTS;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
sub name() { OpenBSD::PackageInfo::CONTENTS }
# XXX we don't write `self'
sub write
{}

sub copy_shallow_if
{
}

sub copy_deep_if
{
}

# CONTENTS doesn't have a checksum
sub may_verify_digest
{
}

package OpenBSD::PackingElement::FDESC;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
sub name() { OpenBSD::PackageInfo::DESC }

package OpenBSD::PackingElement::DisplayFile;
our @ISA=qw(OpenBSD::PackingElement::SpecialFile);
use OpenBSD::Error;

sub prepare
{
	my ($self, $state) = @_;
	my $fname = $self->fullname;
	if (open(my $src, '<', $fname)) {
		while (<$src>) {
			chomp;
			next if m/^\+\-+\s*$/o;
			s/^[+-] //o;
			$state->log("#1", $_);
		}
	} else {
		$state->errsay("Can't open #1: #2", $fname, $!);
    	}
}

package OpenBSD::PackingElement::FDISPLAY;
our @ISA=qw(OpenBSD::PackingElement::DisplayFile);
sub name() { OpenBSD::PackageInfo::DISPLAY }

package OpenBSD::PackingElement::FUNDISPLAY;
our @ISA=qw(OpenBSD::PackingElement::DisplayFile);
sub name() { OpenBSD::PackageInfo::UNDISPLAY }

package OpenBSD::PackingElement::Arch;
our @ISA=qw(OpenBSD::PackingElement::Unique);

sub category() { 'arch' }
sub keyword() { 'arch' }
__PACKAGE__->register_with_factory;

sub new
{
	my ($class, $args) = @_;
	my @arches= split(/\,/o, $args);
	bless { arches => \@arches }, $class;
}

sub stringize($)
{
	my $self = $_[0];
	return join(',', @{$self->{arches}});
}

sub check
{
	my ($self, $forced_arch) = @_;

	for my $ok (@{$self->{arches}}) {
		return 1 if $ok eq '*';
		if (defined $forced_arch) {
			if ($ok eq $forced_arch) {
				return 1;
			} else {
				next;
			}
		}
		return 1 if $ok eq OpenBSD::Paths->machine_architecture;
		return 1 if $ok eq OpenBSD::Paths->architecture;
	}
	return;
}

package OpenBSD::PackingElement::Signer;
our @ISA=qw(OpenBSD::PackingElement::Unique);
sub keyword() { 'signer' }
__PACKAGE__->register_with_factory;
sub category() { "signer" }
sub new
{
	my ($class, $args) = @_;
	unless ($args =~ m/^[\w\d\.\-\+\@]+$/) {
		die "Invalid characters in signer $args\n";
	}
	$class->SUPER::new($args);
}

# don't incorporate this into compared signatures
sub write_without_variation
{
}

# XXX digital-signatures have to be unique, since they are a part
# of the unsigned packing-list, with only the b64sig part removed
# (likewise for signer)
package OpenBSD::PackingElement::DigitalSignature;
our @ISA=qw(OpenBSD::PackingElement::Unique);

sub keyword() { 'digital-signature' }
__PACKAGE__->register_with_factory;
sub category() { "digital-signature" }

# parse to and from a subset of iso8601
#
# allows us to represent timestamps in a human readable format without
# any ambiguity
sub time_to_iso8601
{
	my $time = shift;
	my ($sec, $min, $hour, $day, $month, $year, @rest) = gmtime($time);
	return sprintf("%04d-%02d-%02dT%02d:%02d:%02dZ",
	    $year+1900, $month+1, $day, $hour, $min, $sec);
}

sub iso8601
{
	my $self = shift;
	return time_to_iso8601($self->{timestamp});
}

sub iso8601_to_time
{
	if ($_[0] =~ m/^(\d{4})\-(\d{2})\-(\d{2})T(\d{2})\:(\d{2})\:(\d{2})Z$/) {
		my ($year, $month, $day, $hour, $min, $sec) =
			($1 - 1900, $2-1, $3, $4, $5, $6);
		require POSIX;
		my $oldtz = $ENV{TZ};
		$ENV{TZ} = 'UTC';
		my $t = POSIX::mktime($sec, $min, $hour, $day, $month, $year);
		if (defined $oldtz) {
			$ENV{TZ} = $oldtz;
		} else {
			delete $ENV{TZ};
		}
		return $t;
	} else {
		die "Incorrect ISO8601 timestamp: $_[0]";
	}
}

sub new
{
	my ($class, $args) = @_;
	my ($key, $tsbase, $tsmin, $tssec, $signature) = split(/\:/, $args);
	my $timestamp = iso8601_to_time("$tsbase:$tsmin:$tssec");
	bless { key => $key, timestamp => $timestamp, b64sig => $signature },
		$class;
}

sub blank
{
	my ($class, $type) = @_;
	bless { key => $type, timestamp => time, b64sig => '' }, $class;
}

sub stringize
{
	my $self = shift;
	return join(':', $self->{key}, time_to_iso8601($self->{timestamp}),
	    $self->{b64sig});
}

sub write_no_sig
{
	my ($self, $fh) = @_;
	print $fh "\@", $self->keyword, " ", $self->{key}, ":",
	    time_to_iso8601($self->{timestamp}), "\n";
}

# don't incorporate this into compared signatures
sub write_without_variation
{
}

package OpenBSD::PackingElement::Old;
our @ISA=qw(OpenBSD::PackingElement);

my $warned;

sub new
{
	my ($class, $k, $args) = @_;
	bless { keyword => $k, name => $args }, $class;
}

sub add
{
	my ($o, $plist, $args) = @_;
	my $keyword = $$o;
	if (!$warned->{$keyword}) {
		print STDERR "Warning: obsolete construct: \@$keyword $args\n";
		$warned->{$keyword} = 1;
	}
	my $o2 = OpenBSD::PackingElement::Old->new($keyword, $args);
	$o2->add_object($plist);
	$plist->{deprecated} = 1;
	return undef;
}

sub keyword
{
	my $self = shift;
	return $self->{keyword};
}

sub register_old_keyword
{
	my ($class, $k) = @_;
	$class->register_with_factory($k, bless \$k, $class);
}

for my $k (qw(src display mtree ignore_inst dirrm pkgcfl pkgdep newdepend
    libdepend endfake ignore vendor incompatibility md5 sysctl)) {
	__PACKAGE__->register_old_keyword($k);
}

# Real pkgpath objects, with matching properties
package OpenBSD::PkgPath;
sub new
{
	my ($class, $fullpkgpath) = @_;
	my ($dir, @mandatory) = split(/\,/, $fullpkgpath);
	return bless {dir => $dir,
		mandatory => {map {($_, 1)} @mandatory},
	}, $class;
}

sub fullpkgpath
{
	my ($self) = @_;
	if(%{$self->{mandatory}}) {
		my $m = join(",", keys %{$self->{mandatory}});
		return "$self->{dir},$m";
	} else {
		return $self->{dir};
	}
}

# a pkgpath has a dir, and some flavors/multi parts. To match, we must
# remove them all. So, keep a full hash of everything we have (has), and
# when stuff $to_rm matches, remove them from $from.
# We match when we're left with nothing.
sub trim
{
	my ($self, $has, $from, $to_rm) = @_;
	for my $f (keys %$to_rm) {
		if ($has->{$f}) {
			delete $from->{$f};
		} else {
			return 0;
		}
	}
	return 1;
}

# basic match: after mandatory, nothing left
sub match2
{
	my ($self, $has, $h) = @_;
	if (keys %$h) {
		return 0;
	} else {
		return 1;
	}
}

# zap mandatory, check that what's left is okay.
sub match
{
	my ($self, $other) = @_;
	# make a copy of options
	my %h = %{$other->{mandatory}};
	if (!$self->trim($other->{mandatory}, \%h, $self->{mandatory})) {
		return 0;
	}
	if ($self->match2($other->{mandatory}, \%h)) {
		return 1;
	} else {
		return 0;
	}
}

package OpenBSD::PkgPath::WithOpts;
our @ISA = qw(OpenBSD::PkgPath);

sub new
{
	my ($class, $fullpkgpath) = @_;
	my @opts = ();
	while ($fullpkgpath =~ s/\[\,(.*?)\]//) {
		push(@opts, {map {($_, 1)} split(/\,/, $1) });
	};
	my $o = $class->SUPER::new($fullpkgpath);
	if (@opts == 0) {
		bless $o, "OpenBSD::PkgPath";
	} else {
		$o->{opts} = \@opts;
	}
	return $o;
}

# match with options: systematically trim any optional part that  fully
# matches, until we're left with nothing, or some options keep happening.
sub match2
{
	my ($self, $has, $h) = @_;
	if (!keys %$h) {
		return 1;
	}
	for my $opts (@{$self->{opts}}) {
		my %h2 = %$h;
		if ($self->trim($has, \%h2, $opts)) {
			$h = \%h2;
			if (!keys %$h) {
				return 1;
			}
		}
	}
	return 0;
}

1;
