#! /usr/bin/perl
# ex:ts=8 sw=4:
# $OpenBSD: PkgCreate.pm,v 1.50 2011/10/21 18:19:34 espie Exp $
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

use OpenBSD::AddCreateDelete;
use OpenBSD::Dependencies;
use OpenBSD::SharedLibs;

package OpenBSD::PkgCreate::State;
our @ISA = qw(OpenBSD::AddCreateDelete::State);

sub init
{
	my $self = shift;

	$self->{stash} = {};
	$self->SUPER::init(@_);
	$self->{simple_status} = 0;
}

sub stash
{
	my ($self, $key) = @_;
	return $self->{stash}{$key};
}

sub error
{
	my $self = shift;
	my $msg = shift;
	$self->{bad}++;
	$self->errsay("Error: $msg", @_);
}

sub set_status
{
	my ($self, $status) = @_;
	if ($self->{simple_status}) {
		print "\n$status";
	} else {
		if ($self->progress->set_header($status)) {
			$self->progress->message('');
		} else {
			$| = 1;
			print "$status...";
			$self->{simple_status} = 1;
		}
	}
}

sub end_status
{
	my $self = shift;

	if ($self->{simple_status}) {
		print "\n";
	} else {
		$self->progress->clear;
	}
}

package OpenBSD::PkgCreate;

use OpenBSD::PackingList;
use OpenBSD::PackageInfo;
use OpenBSD::Getopt;
use OpenBSD::Temp;
use OpenBSD::Error;
use OpenBSD::Ustar;
use OpenBSD::ArcCheck;
use OpenBSD::Paths;
use File::Basename;

# Extra stuff needed to archive files
package OpenBSD::PackingElement;
sub create_package
{
	my ($self, $state) = @_;

	$self->archive($state);
	if ($state->verbose) {
		$self->comment_create_package;
	}
}

sub pretend_to_archive
{
	my ($self, $state) = @_;
	$self->comment_create_package;
}

sub archive {}
sub comment_create_package {}
sub grab_manpages {}

sub print_file {}

sub avert_duplicates_and_other_checks
{
	my ($self, $state) = @_;
	return unless $self->NoDuplicateNames;
	my $n = $self->fullname;
	if (defined $state->stash($n)) {
		$state->error("duplicate item in packing-list #1", $n);
	}
	$state->{stash}{$n} = 1;
}

sub makesum_plist
{
	my ($self, $plist, $state) = @_;
	$self->add_object($plist);
}

sub verify_checksum
{
}

sub resolve_link
{
	my ($filename, $base, $level) = @_;
	$level //= 0;
	if (-l $filename) {
		my $l = readlink($filename);
		if ($level++ > 14) {
			return undef;
		}
		if ($l =~ m|^/|) {
			return $base.resolve_link($l, $base, $level);
		} else {
			return resolve_link(File::Spec->catfile(File::Basename::dirname($filename),$l), $base, $level);
		}
	} else {
		return $filename;
	}
}

sub compute_checksum
{
	my ($self, $result, $state, $base) = @_;
	my $name = $self->fullname;
	my $fname = $name;
	if (defined $base) {
		$fname = $base.$fname;
	}
	for my $field (qw(symlink link size)) {  # md5
		if (defined $result->{$field}) {
			$state->error("User tried to define @#1 for #2",
			    $field, $fname);
		}
	}
	if (defined $self->{wtempname}) {
		$fname = $self->{wtempname};
	}
	if (-l $fname) {
		if (!defined $base) {
			$state->error("special file #1 can't be a symlink",
			    $self->stringize);
		}
		my $value = readlink $fname;
		my $chk = resolve_link($fname, $base);
		$fname =~ s|^//|/|; # cosmetic
		if (!defined $chk) {
			$state->error("bogus symlink: #1 (too deep)", $fname);
		} elsif (!-e $chk) {
			push(@{$state->{bad_symlinks}{$chk}}, $fname);
		}
		$result->make_symlink($value);
	} elsif (-f _) {
		my ($dev, $ino, $size) = (stat _)[0,1,7];
		if (defined $state->stash("$dev/$ino")) {
			$result->make_hardlink($state->stash("$dev/$ino"));
		} else {
			$state->{stash}{"$dev/$ino"} = $name;
			$result->add_digest($self->compute_digest($fname));
			$result->add_size($size);
		}
	} elsif (-d _) {
		$state->error("#1 should be a file and not a directory", $fname);
	} else {
		$state->error("#1 does not exist", $fname);
	}
}

sub makesum_plist_with_base
{
	my ($self, $plist, $state, $base) = @_;
	$self->compute_checksum($self, $state, $base);
	$self->add_object($plist);
}

sub verify_checksum_with_base
{
	my ($self, $state, $base) = @_;
	my $check = ref($self)->new($self->name);
	$self->compute_checksum($check, $state, $base);

	for my $field (qw(symlink link size)) {  # md5
		if ((defined $check->{$field} && defined $self->{$field} &&
		    $check->{$field} ne $self->{$field}) ||
		    (defined $check->{$field} xor defined $self->{$field})) {
		    	$state->error("#1 inconsistency for #2",
			    $field, $self->fullname);
		}
	}
	if ((defined $check->{d} && defined $self->{d} &&
	    !$check->{d}->equals($self->{d})) ||
	    (defined $check->{d} xor defined $self->{d})) {
	    	$state->error("checksum inconsistency for #1",
		    $self->fullname);
	}
}


sub prepare_for_archival
{
	my ($self, $state) = @_;

	my $o = $state->{archive}->prepare_long($self);
	if (!$o->verify_modes($self)) {
		$state->error("modes don't match for #1", $self->fullname);
	}
	return $o;
}

sub copy_over
{
}

sub discover_directories
{
}

package OpenBSD::PackingElement::RcScript;
sub archive
{
	my ($self, $state) = @_;
	if ($self->name =~ m/^\//) {
		$state->{archive}->destdir($state->{base});
	}
	$self->SUPER::archive($state);
}

package OpenBSD::PackingElement::SpecialFile;
sub archive
{
	&OpenBSD::PackingElement::FileBase::archive;
}

sub pretend_to_archive
{
	&OpenBSD::PackingElement::FileBase::pretend_to_archive;
}

sub comment_create_package
{
	my ($self) = @_;
	print "Adding ", $self->name, "\n";
}

sub makesum_plist
{
	my ($self, $plist, $state) = @_;
	$self->makesum_plist_with_base($plist, $state, undef);
}

sub verify_checksum
{
	my ($self, $state) = @_;
	$self->verify_checksum_with_base($state, undef);
}

sub prepare_for_archival
{
	my ($self, $state) = @_;

	my $o = $state->{archive}->prepare_long($self);
	$o->{uname} = 'root';
	$o->{gname} = 'wheel';
	$o->{uid} = 0;
	$o->{gid} = 0;
	$o->{mode} &= 0555; # zap all write and suid modes
	return $o;
}

sub copy_over
{
	my ($self, $wrarc, $rdarc) = @_;
	$wrarc->destdir($rdarc->info);
	my $e = $wrarc->prepare($self->{name});
	$e->write;
}

# override for CONTENTS: we cannot checksum this.
package OpenBSD::PackingElement::FCONTENTS;
sub makesum_plist
{
}

sub verify_checksum
{
}


package OpenBSD::PackingElement::Cwd;
sub archive
{
	my ($self, $state) = @_;
	$state->{archive}->destdir($state->{base}."/".$self->name);
}

sub pretend_to_archive
{
	my ($self, $state) = @_;
	$state->{archive}->destdir($state->{base}."/".$self->name);
	$self->comment_create_package;
}

sub comment_create_package
{
	my ($self) = @_;
	print "Cwd: ", $self->name, "\n";
}

package OpenBSD::PackingElement::FileBase;

sub archive
{
	my ($self, $state) = @_;

	my $o = $self->prepare_for_archival($state);

	$o->write unless $state->{bad};
}

sub pretend_to_archive
{
	my ($self, $state) = @_;

	$self->prepare_for_archival($state);
	$self->comment_create_package;
}

sub comment_create_package
{
	my ($self) = @_;
	print "Adding ", $self->name, "\n";
}

sub print_file
{
	my ($item) = @_;
	print '@', $item->keyword, " ", $item->fullname, "\n";
}

sub makesum_plist
{
	my ($self, $plist, $state) = @_;
	$self->makesum_plist_with_base($plist, $state, $state->{base});
}

sub verify_checksum
{
	my ($self, $state) = @_;
	$self->verify_checksum_with_base($state, $state->{base});
}

sub copy_over
{
	my ($self, $wrarc, $rdarc) = @_;
	my $e = $rdarc->next;
	if (!$e->check_name($self)) {
		die "Names don't match: ", $e->{name}, " ", $self->{name};
	}
	$e->copy_long($wrarc);
}

package OpenBSD::PackingElement::Dir;
sub discover_directories
{
	my ($self, $state) = @_;
	$state->{known_dirs}->{$self->fullname} = 1;
}

package OpenBSD::PackingElement::InfoFile;
sub makesum_plist
{
	my ($self, $plist, $state) = @_;
	$self->SUPER::makesum_plist($plist, $state);
	my $fname = $self->fullname;
	for (my $i = 1; ; $i++) {
		if (-e "$state->{base}/$fname-$i") {
			my $e = OpenBSD::PackingElement::File->add($plist, $self->name."-".$i);
			$e->compute_checksum($e, $state, $state->{base});
		} else {
			last;
		}
	}
}

package OpenBSD::PackingElement::Manpage;
use File::Basename;

sub grab_manpages
{
	my ($self, $state) = @_;
	my $filename;
	if ($self->{wtempname}) {
		$filename = $self->{wtempname};
	} else {
		$filename = $state->{base}.$self->fullname;
	}
	push(@{$state->{manpages}}, $filename);
}

sub makesum_plist
{
	my ($self, $plist, $state) = @_;
	if ($state->{subst}->empty("USE_GROFF") || !$self->is_source) {
		return $self->SUPER::makesum_plist($plist, $state);
	}
	my $dest = $self->source_to_dest;
	my $fullname = $self->cwd."/".$dest;
	my $d = dirname($fullname);
	$state->{mandir} //= OpenBSD::Temp::permanent_dir(
	    $ENV{TMPDIR} // '/tmp', "manpage");
	my $tempname = $state->{mandir}."/".$fullname;
	require File::Path;
	File::Path::make_path($state->{mandir}."/".$d);
	open my $fh, ">", $tempname or $state->error("can't create #1: #2", 
	    $tempname, $!);
	chmod 0444, $fh;
	if (-d $state->{base}.$d) {
		undef $d;
	}
	$self->format($state, $tempname, $fh);
	if (-z $tempname) {
		$state->errsay("groff produced empty result for #1", $dest);
		$state->errsay("\tkeeping source manpage");
		return $self->SUPER::makesum_plist($plist, $state);
	}
	if (defined $d && !$state->{known_dirs}->{$d}) {
		$state->{known_dirs}->{$d} = 1;
		OpenBSD::PackingElement::Dir->add($plist, dirname($dest));
	}
	my $e = OpenBSD::PackingElement::Manpage->add($plist, $dest);
	$e->{wtempname} = $tempname;
	$e->compute_checksum($e, $state, $state->{base});
}

package OpenBSD::PackingElement::Depend;
sub avert_duplicates_and_other_checks
{
	my ($self, $state) = @_;
	if (!$self->spec->is_valid) {
		$state->error("invalid \@#1 #2 in packing-list",
		    $self->keyword, $self->stringize);
	}
	$self->SUPER::avert_duplicates_and_other_checks($state);
}

package OpenBSD::PackingElement::Conflict;
sub avert_duplicates_and_other_checks
{
	&OpenBSD::PackingElement::Depend::avert_duplicates_and_other_checks;
}

package OpenBSD::PackingElement::AskUpdate;
sub avert_duplicates_and_other_checks
{
	&OpenBSD::PackingElement::Depend::avert_duplicates_and_other_checks;
}

package OpenBSD::PackingElement::Dependency;
sub avert_duplicates_and_other_checks
{
	my ($self, $state) = @_;

	$self->SUPER::avert_duplicates_and_other_checks($state);

	my @issues = OpenBSD::PackageName->from_string($self->{def})->has_issues;
	if (@issues > 0) {
		$state->error("\@#1 #2\n  #3, #4",
		    $self->keyword, $self->stringize,
		    $self->{def}, join(' ', @issues));
	} elsif ($self->spec->is_valid) {
		my @m = $self->spec->filter($self->{def});
		if (@m == 0) {
			$state->error("\@#1 #2\n  pattern #3 doesn't match default #4\n",
			    $self->keyword, $self->stringize,
			    $self->{pattern}, $self->{def});
		}
	}
}

package OpenBSD::PackingElement::Name;
sub avert_duplicates_and_other_checks
{
	my ($self, $state) = @_;

	my @issues = OpenBSD::PackageName->from_string($self->name)->has_issues;
	if (@issues > 0) {
		$state->error("bad package name #1: ", $self->name,
		    join(' ', @issues));
	}
	$self->SUPER::avert_duplicates_and_other_checks($state);
}

# put together file and filename, in order to handle fragments simply
package MyFile;
sub new
{
	my ($class, $filename) = @_;

	open(my $fh, '<', $filename) or die "Missing file $filename";

	bless { fh => $fh, name => $filename }, $class;
}

sub readline
{
	my $self = shift;
	return readline $self->{fh};
}

sub name
{
	my $self = shift;
	return $self->{name};
}

sub close
{
	my $self = shift;
	close($self->{fh});
}

# special solver class for PkgCreate
package OpenBSD::Dependencies::CreateSolver;
our @ISA = qw(OpenBSD::Dependencies::SolverBase);

# we need to "hack" a special set
sub new
{
	my ($class, $plist) = @_;
	bless { set => OpenBSD::PseudoSet->new($plist), bad => [] }, $class;
}

sub solve_all_depends
{
	my ($solver, $state) = @_;


	while (1) {
		my @todo = $solver->solve_depends($state);
		if (@todo == 0) {
			return;
		}
		$solver->{set}->add_new(@todo);
	}
}

sub really_solve_dependency
{
	my ($self, $state, $dep, $package) = @_;

	$state->progress->message($dep->{pkgpath});

	# look in installed packages
	my $v = $self->find_dep_in_installed($state, $dep);
	if (!defined $v) {
		$v = $self->find_dep_in_self($state, $dep);
	}

	# and in portstree otherwise
	if (!defined $v) {
		$v = $self->solve_from_ports($state, $dep, $package);
	}
	return $v;
}

my $cache = {};
sub solve_from_ports
{
	my ($self, $state, $dep, $package) = @_;

	my $portsdir = $state->defines('PORTSDIR');
	return undef unless defined $portsdir;
	my $pkgname;
	if (defined $cache->{$dep->{pkgpath}}) {
		$pkgname = $cache->{$dep->{pkgpath}};
	} else {
		my $plist = $self->ask_tree($state, $dep, $portsdir,
		    'print-plist-with-depends', 'wantlib_args=no-wantlib-args');
		if ($? != 0 || !defined $plist->pkgname) {
			$state->error("Can't obtain dependency #1 from ports tree",
			    $dep->{pattern});
			return undef;
		}
		OpenBSD::SharedLibs::add_libs_from_plist($plist, $state);
		$self->add_dep($plist);
		$pkgname = $plist->pkgname;
		$cache->{$dep->{pkgpath}} = $pkgname;
	}
	if ($dep->spec->filter($pkgname) == 0) {
		$state->error("Dependency #1 doesn't match FULLPKGNAME: #2",
		    $dep->{pattern}, $pkgname);
		return undef;
	}

	return $pkgname;
}

sub ask_tree
{
	my ($self, $state, $dep, $portsdir, @action) = @_;

	my $make = OpenBSD::Paths->make;
	my $pid = open(my $fh, "-|");
	if (!defined $pid) {
		$state->fatal("cannot fork: $!");
	}
	if ($pid == 0) {
		chdir $portsdir or exit 2;
		open STDERR, '>', '/dev/null';
		$ENV{FULLPATH} = 'Yes';
		$ENV{SUBDIR} = $dep->{pkgpath};
		$ENV{ECHO_MSG} = ':';
		exec $make ('make', @action);
	}
	my $plist = OpenBSD::PackingList->read($fh,
	    \&OpenBSD::PackingList::PrelinkStuffOnly);
	close($fh);
	return $plist;
}

sub errsay_library
{
	my ($solver, $state, $h) = @_;

	$state->errsay("Can't create #1 because of libraries", $h->pkgname);
}

# we don't want old libs
sub find_old_lib
{
	return undef;
}

package OpenBSD::PseudoHandle;
sub new
{
	my ($class, $plist) = @_;
	bless { plist => $plist}, $class;
}

sub pkgname
{
	my $self = shift;

	return $self->{plist}->pkgname;
}

package OpenBSD::PseudoSet;
sub new
{
	my ($class, @elements) = @_;

	my $o = bless {}, $class;
	$o->add_new(@elements);
}

sub add_new
{
	my ($self, @elements) = @_;
	for my $i (@elements) {
		push(@{$self->{new}}, OpenBSD::PseudoHandle->new($i));
	}
	return $self;
}

sub newer
{
	return @{shift->{new}};
}


sub newer_names
{
	return map {$_->pkgname} @{shift->{new}};
}

sub older
{
	return ();
}

sub older_names
{
	return ();
}

sub kept
{
	return ();
}

sub print
{
	my $self = shift;
	return $self->{new}[0]->pkgname;
}

package OpenBSD::PkgCreate;
our @ISA = qw(OpenBSD::AddCreateDelete);

sub deduce_name
{
	my ($state, $o, $frag, $not) = @_;

	my $noto = $o;
	my $nofrag = "no-$frag";

	$o =~ s/PFRAG\./PFRAG.$frag-/o or
	    $o =~ s/PLIST/PFRAG.$frag/o;

	$noto =~ s/PFRAG\./PFRAG.no-$frag-/o or
	    $noto =~ s/PLIST/PFRAG.no-$frag/o;
	unless (-e $o or -e $noto) {
		die "Missing fragments for $frag: $o and $noto don't exist";
	}
	if ($not) {
		return $noto if -e $noto;
    	} else {
		return $o if -e $o;
	}
	return;
}

sub handle_fragment
{
	my ($state, $stack, $file, $not, $frag) = @_;
	my $def = $frag;
	if ($frag eq 'SHARED') {
		$def = 'SHARED_LIBS';
		$frag = 'shared';
	}
	my $newname = deduce_name($state, $file->name, $frag, $not);
	if ($state->{subst}->has_fragment($def, $frag)) {
		return $file if defined $not;
	} else {
		return $file unless defined $not;
	}
	if (defined $newname) {
		$state->set_status("switching to $newname")
		    if !defined $state->opt('q');
		push(@$stack, $file);
		$file = MyFile->new($newname);
	}
	return $file;
}

sub read_fragments
{
	my ($state, $plist, $filename) = @_;

	my $stack = [];
	my $subst = $state->{subst};
	push(@$stack, MyFile->new($filename));
	my $fast = $subst->value("LIBS_ONLY");

	return $plist->read($stack,
	    sub {
		my ($stack, $cont) = @_;
		while(my $file = pop @$stack) {
			while (my $_ = $file->readline) {
				$state->progress->working(2048) unless $state->opt('q');
				if (m/^(\@comment\s+\$(?:Open)BSD\$)$/o) {
					$_ = '@comment $'.'OpenBSD: '.basename($file->name).',v$';
				}
				if (m/^\@lib\s+(.*)$/o &&
				    OpenBSD::PackingElement::Lib->parse($1)) {
				    	$state->error("shared library without SHARED_LIBS: #1", $_);
				}
				if (my ($not, $frag) = m/^(\!)?\%\%(.*)\%\%$/) {
					$file = handle_fragment($state, $stack,
					    $file, $not, $frag);
				} else {
					$_ = $subst->do($_);
					if ($fast) {
						next unless m/^\@(?:cwd|lib)\b/o || m/lib.*\.a$/o;
					}
					&$cont($_);
				}
			}
		}
	    });
}

sub add_special_file
{
	my ($subst, $plist, $name, $opt) = @_;
	if (defined $opt) {
	    my $o = OpenBSD::PackingElement::File->add($plist, $name);
	    $subst->copy($opt, $o->fullname) if defined $o->fullname;
	}
}

sub add_description
{
	my ($state, $plist, $name, $opt_d) = @_;
	my $o = OpenBSD::PackingElement::FDESC->add($plist, $name);
	my $subst = $state->{subst};
	my $comment = $subst->value('COMMENT');
	if (defined $comment) {
		if (length $comment > 60) {
			$state->fatal("comment is too long\n#1\n#2\n",
			    $comment, ' 'x60 . "^" x (length($comment)-60));
		}
	} else {
		$state->usage("Comment required");
	}
	if (!defined $opt_d) {
		$state->usage("Description required");
	}
	if (defined $o->fullname) {
	    open(my $fh, '>', $o->fullname) or die "Can't write to DESC: $!";
	    if (defined $comment) {
	    	print $fh $subst->do($comment), "\n";
	    }
	    if ($opt_d =~ /^\-(.*)$/o) {
		print $fh $1, "\n";
	    } else {
		$subst->copy_fh($opt_d, $fh);
	    }
	    if (defined $comment) {
		if ($subst->empty('MAINTAINER')) {
			$state->errsay("no MAINTAINER");
		} else {
			print $fh "\n", $subst->do('Maintainer: ${MAINTAINER}'), "\n";
		}
		if (!$subst->empty('HOMEPAGE')) {
			print $fh "\n", $subst->do('WWW: ${HOMEPAGE}'), "\n";
		}
	    }
	    close($fh);
	}
}

sub add_signature
{
	my ($self, $plist, $cert, $privkey) = @_;

	require OpenBSD::x509;

	my $sig = OpenBSD::PackingElement::DigitalSignature->new_x509;
	$sig->add_object($plist);
	$sig->{b64sig} = OpenBSD::x509::compute_signature($plist,
	    $cert, $privkey);
}

sub create_archive
{
	my ($self, $state, $filename, $dir) = @_;
	open(my $fh, "|-", OpenBSD::Paths->gzip, "-f", "-o", $filename);
	return  OpenBSD::Ustar->new($fh, $state, $dir);
}

sub sign_existing_package
{
	my ($self, $state, $pkgname, $cert, $privkey) = @_;


	my $true_package = $state->repo->find($pkgname);
	$state->fatal("No such package #1", $pkgname) unless $true_package;
	my $dir = $true_package->info;
	my $plist = OpenBSD::PackingList->fromfile($dir.CONTENTS);
	$plist->set_infodir($dir);
	$self->add_signature($plist, $cert, $privkey);
	$plist->save;
	my $tmp = OpenBSD::Temp::permanent_file(".", "pkg");
	my $wrarc = $self->create_archive($state, $tmp, ".");
	$plist->copy_over($wrarc, $true_package);
	$wrarc->close;
	$true_package->wipe_info;
	unlink($plist->pkgname.".tgz");
	rename($tmp, $plist->pkgname.".tgz") or
	    $state->fatal("Can't create final signed package: #1", $!);
}

sub add_extra_info
{
	my ($self, $plist, $state) = @_;

	my $subst = $state->{subst};
	my $fullpkgpath = $subst->value('FULLPKGPATH');
	my $cdrom = $subst->value('PERMIT_PACKAGE_CDROM') ||
	    $subst->value('CDROM');;
	my $ftp = $subst->value('PERMIT_PACKAGE_FTP') ||
	    $subst->value('FTP');
	if (defined $fullpkgpath || defined $cdrom || defined $ftp) {
		$fullpkgpath //= '';
		$cdrom //= 'no';
		$ftp //= 'no';
		$cdrom = 'yes' if $cdrom =~ m/^yes$/io;
		$ftp = 'yes' if $ftp =~ m/^yes$/io;

		OpenBSD::PackingElement::ExtraInfo->add($plist,
		    $fullpkgpath, $cdrom, $ftp);
	} else {
		$state->errsay("Package without FULLPKGPATH");
	}
}

sub add_elements
{
	my ($self, $plist, $state, $dep, $want) = @_;

	my $subst = $state->{subst};
	add_description($state, $plist, DESC, $state->opt('d'));
	add_special_file($subst, $plist, DISPLAY, $state->opt('M'));
	add_special_file($subst, $plist, UNDISPLAY, $state->opt('U'));
	if (defined $state->opt('p')) {
		OpenBSD::PackingElement::Cwd->add($plist, $state->opt('p'));
	} else {
		$state->usage("Prefix required");
	}
	for my $d (sort keys %$dep) {
		OpenBSD::PackingElement::Dependency->add($plist, $d);
	}

	for my $w (sort keys %$want) {
		OpenBSD::PackingElement::Wantlib->add($plist, $w);
	}

	if (defined $state->opt('A')) {
		OpenBSD::PackingElement::Arch->add($plist, $state->opt('A'));
	}

	if (defined $state->opt('L')) {
		OpenBSD::PackingElement::LocalBase->add($plist, $state->opt('L'));
	}
	$self->add_extra_info($plist, $state);
}

sub create_plist
{
	my ($self, $state, $pkgname, $frags, $dep, $want) = @_;

	my $plist = OpenBSD::PackingList->new;

	if ($pkgname =~ m|([^/]+)$|o) {
		$pkgname = $1;
		$pkgname =~ s/\.tgz$//o;
	}
	$plist->set_pkgname($pkgname);
	$state->say("Creating package #1", $pkgname)
	    if !(defined $state->opt('q')) && $state->opt('v');
	if (!$state->opt('q')) {
		$plist->set_infodir(OpenBSD::Temp->dir);
	}

	$self->add_elements($plist, $state, $dep, $want);
	unless (defined $state->opt('q') && defined $state->opt('n')) {
		$state->set_status("reading plist");
	}
	for my $contentsfile (@$frags) {
		read_fragments($state, $plist, $contentsfile) or
		    $state->fatal("can't read packing-list #1", $contentsfile);
	}
	return $plist;
}

sub make_plist_with_sum
{
	my ($self, $state, $plist) = @_;
	my $p2 = OpenBSD::PackingList->new;
	$state->progress->visit_with_count($plist, 'makesum_plist', $p2, $state);
	$p2->set_infodir($plist->infodir);
	return $p2;
}

sub read_existing_plist
{
	my ($self, $state, $contents) = @_;

	my $plist = OpenBSD::PackingList->new;
	if (-d $contents && -f $contents.'/'.CONTENTS) {
		$plist->set_infodir($contents);
		$contents .= '/'.CONTENTS;
	} else {
		$plist->set_infodir(dirname($contents));
	}
	$plist->fromfile($contents) or
	    $state->fatal("can't read packing-list #1", $contents);
	return $plist;
}

sub create_package
{
	my ($self, $state, $plist, $wname) = @_;

	$state->say("Creating gzip'd tar ball in '#1'", $wname)
	    if $state->opt('v');
	my $h = sub {
		unlink $wname;
		my $caught = shift;
		$SIG{$caught} = 'DEFAULT';
		kill $caught, $$;
	};

	local $SIG{'INT'} = $h;
	local $SIG{'QUIT'} = $h;
	local $SIG{'HUP'} = $h;
	local $SIG{'KILL'} = $h;
	local $SIG{'TERM'} = $h;
	$state->{archive} = $self->create_archive($state, $wname,
	    $plist->infodir);
	$state->set_status("archiving");
	$state->progress->visit_with_size($plist, 'create_package', $state);
	$state->end_status;
	$state->{archive}->close;
	if ($state->{bad}) {
		unlink($wname);
		exit(1);
	}
}

sub show_bad_symlinks
{
	my ($self, $state) = @_;
	for my $dest (sort keys %{$state->{bad_symlinks}}) {
		$state->errsay("Warning: symlink(s) point to non-existent #1",
		    $dest);
		for my $link (@{$state->{bad_symlinks}{$dest}}) {
			$state->errsay("\t#1", $link);
		}
	}
}

sub check_dependencies
{
	my ($self, $plist, $state) = @_;

	my $solver = OpenBSD::Dependencies::CreateSolver->new($plist);
	$solver->solve_all_depends($state);
	# look for libraries in the "real" tree
	$state->{destdir} = '/';
	if (!$solver->solve_wantlibs($state)) {
		$state->{bad}++;
	}
}

sub finish_manpages
{
	my ($self, $state, $plist) = @_;
	$plist->grab_manpages($state);
	if (defined $state->{manpages}) {
		$state->{v} ++;

		require OpenBSD::Makewhatis;

		try {
			OpenBSD::Makewhatis::scan_manpages($state->{manpages}, 
			    $state);
		} catchall {
			$state->errsay("Error in makewhatis: #1", $_);
		};
		$state->{v} --;
	}

	if (defined $state->{mandir}) {
		require File::Path;
		File::Path::remove_tree($state->{mandir});
	}
}

sub parse_and_run
{
	my ($self, $cmd) = @_;

	my ($cert, $privkey);
	my $regen_package = 0;
	my $sign_only = 0;
	my (@contents, %dependencies, %wantlib, @signature_params);


	my $state = OpenBSD::PkgCreate::State->new($cmd);

	$state->{opt} = {
	    'f' =>
		    sub {
			    push(@contents, shift);
		    },
	    'P' => sub {
			    my $d = shift;
			    $dependencies{$d} = 1;
		    },
	    'W' => sub {
			    my $w = shift;
			    $wantlib{$w} = 1;
		    },
	    's' => sub {
			    push(@signature_params, shift);
		    }
	};
	$state->{no_exports} = 1;
	$state->handle_options('p:f:d:M:U:s:A:B:P:W:qQ',
	    '[-nQqvx] [-A arches] [-B pkg-destdir] [-D name[=value]]',
	    '[-L localbase] [-M displayfile] [-P pkg-dependency]',
	    '[-s x509 -s cert -s priv] [-U undisplayfile] [-W wantedlib]',
	    '-d desc -D COMMENT=value -f packinglist -p prefix pkg-name');

	if (@ARGV == 0) {
		$regen_package = 1;
	} elsif (@ARGV != 1) {
		if (@contents || @signature_params == 0) {
			$state->usage("Exactly one single package name is required: #1", join(' ', @ARGV));
		}
	}

	try {
	if (@signature_params > 0) {
		if (@signature_params != 3 || $signature_params[0] ne 'x509' ||
		    !-f $signature_params[1] || !-f $signature_params[2]) {
			$state->usage("Signature only works as -s x509 -s cert -s privkey");
		}
		$cert = $signature_params[1];
		$privkey = $signature_params[2];
	}

	if (defined $state->opt('Q')) {
		$state->{opt}{q} = 1;
	}

	if (!@contents) {
		if (@signature_params > 0) {
			$sign_only = 1;
		} else {
			$state->usage("Packing-list required");
		}
	}

	my $plist;
	if ($regen_package) {
		if (@contents != 1) {
			$state->usage("Exactly one single packing-list is required");
		}
		$plist = $self->read_existing_plist($state, $contents[0]);
	} elsif ($sign_only) {
		if ($state->not) {
			$state->fatal("can't pretend to sign existing packages");
		}
		for my $pkgname (@ARGV) {
			$self->sign_existing($state, $pkgname, $cert, $privkey);
		}
		return 0;
	} else {
		$plist = $self->create_plist($state, $ARGV[0], \@contents,
		    \%dependencies, \%wantlib);
	}


	my $base = '/';
	if (defined $state->opt('B')) {
		$base = $state->opt('B');
	} elsif (defined $ENV{'PKG_PREFIX'}) {
		$base = $ENV{'PKG_PREFIX'};
	}

	$state->{base} = $base;

	$plist->discover_directories($state);
	unless (defined $state->opt('q') && defined $state->opt('n')) {
		$state->set_status("checking dependencies");
		$self->check_dependencies($plist, $state);
		$state->set_status("checksumming");
		if ($regen_package) {
			$state->progress->visit_with_count($plist, 'verify_checksum', $state);
		} else {
			$plist = $self->make_plist_with_sum($state, $plist);
		}
		$self->show_bad_symlinks($state);
		$state->end_status;
	}

	if (!defined $plist->pkgname) {
		$state->fatal("can't write unnamed packing-list");
	}

	if (defined $state->opt('q')) {
		if (defined $state->opt('Q')) {
			$plist->print_file;
		} else {
			$plist->write(\*STDOUT);
		}
		return 0 if defined $state->opt('n');
	}

	if ($plist->{deprecated}) {
		$state->fatal("found obsolete constructs");
	}

	$plist->avert_duplicates_and_other_checks($state);
	$state->{stash} = {};

	if ($state->{bad} && !$state->defines('REGRESSION_TESTING')) {
		$state->fatal("can't continue");
	}
	$state->{bad} = 0;

	if (defined $cert) {
		$self->add_signature($plist, $cert, $privkey);
		$plist->save if $regen_package;
	}

	my $wname;
	if ($regen_package) {
		$wname = $plist->pkgname.".tgz";
	} else {
		$plist->save or $state->fatal("can't write packing-list");
		$wname = $ARGV[0];
	}

	if ($state->opt('n')) {
		$state->{archive} = OpenBSD::Ustar->new(undef, $state,
		    $plist->infodir);
		$plist->pretend_to_archive($state);
	} else {
		$self->create_package($state, $plist, $wname);
	}
	$self->finish_manpages($state, $plist);
	}catch {
		print STDERR "$0: $_\n";
		return 1;
	};
	return 0;
}

1;
