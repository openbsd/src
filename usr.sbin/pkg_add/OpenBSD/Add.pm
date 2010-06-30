# ex:ts=8 sw=4:
# $OpenBSD: Add.pm,v 1.113 2010/06/30 10:51:04 espie Exp $
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

package OpenBSD::Add;
use OpenBSD::Error;
use OpenBSD::PackageInfo;
use OpenBSD::ArcCheck;
use OpenBSD::Paths;
use File::Copy;

sub manpages_index
{
	my ($state) = @_;
	return unless defined $state->{mandirs};
	my $destdir = $state->{destdir};
	require OpenBSD::Makewhatis;

	while (my ($k, $v) = each %{$state->{mandirs}}) {
		my @l = map { $destdir.$_ } @$v;
		if ($state->{not}) {
			$state->say("Merging manpages in #1: #2",
				$destdir.$k, join(@l)) if $state->verbose >= 2;
		} else {
			try {
				OpenBSD::Makewhatis::merge($destdir.$k, \@l);
			} catchall {
				$state->errsay("Error in makewhatis: #1", $_);
			};
		}
	}
}

sub register_installation
{
	my $plist = shift;
	return if $main::not;
	my $dest = installed_info($plist->pkgname);
	mkdir($dest);
	$plist->copy_info($dest);
	$plist->set_infodir($dest);
	$plist->to_installation;
}

sub validate_plist
{
	my ($plist, $state, $set) = @_;

	$plist->prepare_for_addition($state, $plist->pkgname, $set);
}

sub record_partial_installation
{
	my ($plist, $state, $h) = @_;

	use OpenBSD::PackingElement;

	my $n = $plist->make_shallow_copy($h);
	my $borked = borked_package($plist->pkgname);
	$n->set_pkgname($borked);

	# last file may have not copied correctly
	my $last = $n->{state}->{lastfile};
	if (defined $last && defined($last->{d})) {

	    my $old = $last->{d};
	    my $lastname = $last->realname($state);
	    $last->{d} = $last->compute_digest($lastname, $old);
	    if (!$old->equals($last->{d})) {
		$state->say("Adjusting #1 for #2 from #3 to #4",
		    $old->keyword, $lastname, $old->stringize,
		    $last->{d}->stringize);
	    }
	}
	register_installation($n);
	return $borked;
}

sub perform_installation
{
	my ($handle, $state) = @_;

	$state->{archive} = $handle->{location};
	$state->{end_faked} = 0;
	$handle->{partial} //= {};
	$state->{partial} = $handle->{partial};
	$state->progress->visit_with_size($handle->{plist}, 'install', $state);
	$handle->{location}->finish_and_close;
}

my $user_tagged = {};

sub extract_pkgname
{
	my $pkgname = shift;
	$pkgname =~ s/^.*\///;
	$pkgname =~ s/\.tgz$//;
	return $pkgname;
}

sub tweak_package_status
{
	my ($pkgname, $state) = @_;

	$pkgname = extract_pkgname($pkgname);
	return 0 unless is_installed($pkgname);
	return 0 unless $user_tagged->{$pkgname};
	my $plist = OpenBSD::PackingList->from_installation($pkgname);
	if ($plist->has('manual-installation') && $state->{automatic}) {
		delete $plist->{'manual-installation'};
		$plist->to_installation;
		return 1;
	} elsif (!$plist->has('manual-installation') && !$state->{automatic}) {
		OpenBSD::PackingElement::ManualInstallation->add($plist);
		$plist->to_installation;
		return 1;
	}
	return 0;
}

sub tweak_plist_status
{
	my ($plist, $state) = @_;

	my $pkgname = $plist->pkgname;
	return 0 unless $user_tagged->{$pkgname};
	if (!$plist->has('manual-installation') && !$state->{automatic}) {
		OpenBSD::PackingElement::ManualInstallation->add($plist);
	}
}

sub tag_user_packages
{
	for my $set (@_) {
		for my $n ($set->newer_names) {
			$user_tagged->{OpenBSD::PackageName::url2pkgname($n)} = 1;
		}
	}
}

# used by newuser/newgroup to deal with options.
package OpenBSD::PackingElement;
use OpenBSD::Error;

my ($uidcache, $gidcache);

sub prepare_for_addition
{
}

sub install
{
	my ($self, $state) = @_;
	if ($state->{interrupted}) {
		die "Interrupted";
	}
	$state->{partial}->{$self} = 1;
}

sub copy_info
{
}

sub set_modes
{
	my ($self, $state, $name) = @_;

	if (defined $self->{owner} || defined $self->{group}) {
		require OpenBSD::IdCache;

		if (!defined $uidcache) {
			$uidcache = OpenBSD::UidCache->new;
			$gidcache = OpenBSD::GidCache->new;
		}
		my ($uid, $gid);
		if (-l $name) {
			($uid, $gid) = (lstat $name)[4,5];
		} else {
			($uid, $gid) = (stat $name)[4,5];
		}
		if (defined $self->{owner}) {
			$uid = $uidcache->lookup($self->{owner}, $uid);
		}
		if (defined $self->{group}) {
			$gid = $gidcache->lookup($self->{group}, $gid);
		}
		chown $uid, $gid, $name;
	}
	if (defined $self->{mode}) {
		my $v = $self->{mode};
		if ($v =~ m/^\d+$/o) {
			chmod oct($v), $name;
		} else {
			$state->system(OpenBSD::Paths->chmod, $self->{mode}, $name);
		}
	}
}

package OpenBSD::PackingElement::ExtraInfo;
use OpenBSD::Error;

sub prepare_for_addition
{
	my ($self, $state, $pkgname) = @_;

	if ($state->{cdrom_only} && $self->{cdrom} ne 'yes') {
	    $state->errsay("Package #1 is not for cdrom", $pkgname);
	    $state->{problems}++;
	}
	if ($state->{ftp_only} && $self->{ftp} ne 'yes') {
	    $state->errsay("Package #1 is not for ftp", $pkgname);
	    $state->{problems}++;
	}
}

package OpenBSD::PackingElement::NewAuth;
use OpenBSD::Error;

sub add_entry
{
	shift;	# get rid of self
	my $l = shift;
	while (@_ >= 2) {
		my $f = shift;
		my $v = shift;
		next if !defined $v or $v eq '';
		if ($v =~ m/^\!(.*)$/o) {
			push(@$l, $f, $1);
		} else {
			push(@$l, $f, $v);
		}
	}
}

sub prepare_for_addition
{
	my ($self, $state, $pkgname) = @_;
	my $ok = $self->check;
	if (defined $ok) {
		if ($ok == 0) {
			$state->errsay("#1 #2 does not match",
			    $self->type, $self->name);
			$state->{problems}++;
		}
	}
	$self->{okay} = $ok;
}

sub install
{
	my ($self, $state) = @_;
	$self->SUPER::install($state);
	my $auth = $self->name;
	$state->say("adding #1 #2", $self->type, $auth) if $state->verbose >= 2;
	return if $state->{not};
	return if defined $self->{okay};
	my $l=[];
	push(@$l, "-v") if $state->verbose >= 2;
	$self->build_args($l);
	$state->vsystem($self->command,, @$l, '--', $auth);
}

package OpenBSD::PackingElement::NewUser;

sub command 	{ OpenBSD::Paths->useradd }

sub build_args
{
	my ($self, $l) = @_;

	$self->add_entry($l,
	    '-u', $self->{uid},
	    '-g', $self->{group},
	    '-L', $self->{class},
	    '-c', $self->{comment},
	    '-d', $self->{home},
	    '-s', $self->{shell});
}

package OpenBSD::PackingElement::NewGroup;

sub command { OpenBSD::Paths->groupadd }

sub build_args
{
	my ($self, $l) = @_;

	$self->add_entry($l, '-g', $self->{gid});
}

package OpenBSD::PackingElement::Sysctl;
use OpenBSD::Error;

sub install
{
	my ($self, $state) = @_;

	my $name = $self->name;
	$self->SUPER::install($state);
	open(my $pipe, '-|', OpenBSD::Paths->sysctl, '-n', '--', $name);
	my $actual = <$pipe>;
	chomp $actual;
	if ($self->{mode} eq '=' && $actual eq $self->{value}) {
		return;
	}
	if ($self->{mode} eq '>=' && $actual >= $self->{value}) {
		return;
	}
	if ($state->{not}) {
		$state->say("sysctl -w #1 =! #2",
		    $name, $self->{value}) if $state->verbose >= 2;
		return;
	}
	$state->vsystem(OpenBSD::Paths->sysctl, '--', $name.'='.$self->{value});
}

package OpenBSD::PackingElement::DirBase;
sub prepare_for_addition
{
	my ($self, $state, $pkgname) = @_;
	return unless $self->{noshadow};
	$state->{noshadow}->{$state->{destdir}.$self->fullname} = 1;
}

package OpenBSD::PackingElement::FileBase;
use OpenBSD::Error;
use File::Basename;
use File::Path;

sub prepare_for_addition
{
	my ($self, $state, $pkgname) = @_;
	my $fname = $state->{destdir}.$self->fullname;
	# check for collisions with existing stuff
	if ($state->vstat->exists($fname)) {
		push(@{$state->{colliding}}, $self);
		$state->{problems}++;
		return;
	}
	my $s = $state->vstat->add($fname, $self->{size}, $pkgname);
	return unless defined $s;
	if ($s->ro) {
		$s->report_ro($state, $fname);
	}
	if ($s->avail < 0) {
		$s->report_overflow($state, $fname);
	}
}

sub install
{
	my ($self, $state) = @_;
	$self->SUPER::install($state);
	my $fullname = $self->fullname;
	my $destdir = $state->{destdir};

	if ($state->{extracted_first}) {
		if ($state->{not}) {
			$state->say("moving tempfile -> #1",
			    $destdir.$fullname) if $state->verbose >= 5;
			return;
		}
		File::Path::mkpath(dirname($destdir.$fullname));
		if (defined $self->{link}) {
			link($destdir.$self->{link}, $destdir.$fullname);
		} elsif (defined $self->{symlink}) {
			symlink($self->{symlink}, $destdir.$fullname);
		} else {
			rename($self->{tempname}, $destdir.$fullname) or
			    $state->fatal("can't move #1 to #2: #3",
			    	$self->{tempname}, $fullname, $!);
			$state->say("moving #1 -> #2",
			    $self->{tempname}, $destdir.$fullname)
			    	if $state->verbose >= 5;
			undef $self->{tempname};
		}
	} else {
		my $file = $self->prepare_to_extract($state);

		$state->say("extracting #1", $destdir.$fullname)
		    if $state->verbose >= 5;
		if ($state->{not}) {
			$state->{archive}->skip;
			return;
		} else {
			$file->create;
			$self->may_check_digest($file, $state);

		}
	}
	$self->set_modes($state, $destdir.$fullname);
}

sub prepare_to_extract
{
	my ($self, $state) = @_;
	my $fullname = $self->fullname;
	my $destdir = $state->{destdir};

	my $file=$state->{archive}->next;
	if (!defined $file) {
		$state->fatal("truncated archive");
	}
	$file->{cwd} = $self->cwd;
	if (!$file->check_name($self)) {
		$state->fatal("archive does not match #1 != #2",
		    $file->name, $self->name);
	}
	if (defined $self->{symlink} || $file->isSymLink) {
		unless (defined $self->{symlink} && $file->isSymLink) {
			$state->fatal("bogus symlink #1", $self->name);
		}
		if (!$file->check_linkname($self->{symlink})) {
			$state->fatal("archive symlink does not match #1 != #2",
			    $file->{linkname}, $self->{symlink});
		}
	} elsif (defined $self->{link} || $file->isHardLink) {
		unless (defined $self->{link} && $file->isHardLink) {
			$state->fatal("bogus hardlink #1", $self->name);
		}
		if (!$file->check_linkname($self->{link})) {
			$state->fatal("archive hardlink does not match #1 != #2",
			    $file->{linkname}, $self->{link});
		}
	}
	if (!$file->verify_modes($self)) {
		$state->fatal("can't continue");
	}

	$file->set_name($fullname);
	$file->{destdir} = $destdir;
	# faked installation are VERY weird
	if (defined $self->{symlink} && $state->{do_faked}) {
		$file->{linkname} = $destdir.$file->{linkname};
	}
	return $file;
}

package OpenBSD::PackingElement::EndFake;
sub install
{
	my ($self, $state) = @_;

	$self->SUPER::install($state);
	$state->{end_faked} = 1;
}

package OpenBSD::PackingElement::Sample;
use OpenBSD::Error;
use File::Copy;

sub prepare_for_addition
{
	my ($self, $state, $pkgname) = @_;
	if (!defined $self->{copyfrom}) {
		$state->errsay("\@sample element #1 does not reference a valid file",
		    $self->fullname);
		$state->{problems}++;
	}
	my $fname = $state->{destdir}.$self->fullname;
	# If file already exists, we won't change it
	if ($state->vstat->exists($fname)) {
		return;
	}
	my $size = $self->{copyfrom}->{size};
	my $s = $state->vstat->add($fname, $size, $pkgname);
	return unless defined $s;
	if ($s->ro) {
		$s->report_ro($state, $fname);
	}
	if ($s->avail < 0) {
		$s->report_overflow($state, $fname);
	}
}

sub install
{
	my ($self, $state) = @_;

	$self->SUPER::install($state);
	my $destdir = $state->{destdir};
	my $filename = $destdir.$self->fullname;
	my $orig = $self->{copyfrom};
	my $origname = $destdir.$orig->fullname;
	if (-e $filename) {
		if ($state->verbose) {
		    $state->say("The existing file #1 has NOT been changed",
		    	$filename);
		    if (defined $orig->{d}) {

			# XXX assume this would be the same type of file
			my $d = $self->compute_digest($filename, $orig->{d});
			if ($d->equals($orig->{d})) {
			    $state->say("(but it seems to match the sample file #1)", $origname);
			} else {
			    $state->say("It does NOT match the sample file #1",
				$origname);
			    $state->say("You may wish to update it manually");
			}
		    }
		}
	} else {
		if ($state->{not}) {
			$state->say("The file #1 would be installed from #2",
			    $filename, $origname) if $state->verbose >= 2;
		} else {
			if (!copy($origname, $filename)) {
				$state->errsay("File #1 could not be installed:\n\t#2", $filename, $!);
			}
			$self->set_modes($state, $filename);
			if ($state->verbose >= 2) {
			    $state->say("installed #1 from #2",
				$filename, $origname);
			}
		}
	}
}

package OpenBSD::PackingElement::Sampledir;

sub install
{
	&OpenBSD::PackingElement::Dir::install;
}

package OpenBSD::PackingElement::Mandir;

sub install
{
	my ($self, $state) = @_;
	$self->SUPER::install($state);
	$state->log("You may wish to add #1 to /etc/man.conf", $self->fullname);
}

package OpenBSD::PackingElement::Manpage;

sub install
{
	my ($self, $state) = @_;
	$self->SUPER::install($state);
	$self->register_manpage($state) unless $state->{not};
}

package OpenBSD::PackingElement::InfoFile;
use File::Basename;
use OpenBSD::Error;

sub install
{
	my ($self, $state) = @_;
	$self->SUPER::install($state);
	return if $state->{not};
	my $fullname = $state->{destdir}.$self->fullname;
	$state->vsystem(OpenBSD::Paths->install_info,
	    "--info-dir=".dirname($fullname), '--', $fullname);
}

package OpenBSD::PackingElement::Shell;
sub install
{
	my ($self, $state) = @_;
	$self->SUPER::install($state);
	return if $state->{not};
	my $fullname = $self->fullname;
	my $destdir = $state->{destdir};
	# go append to /etc/shells if needed
	open(my $shells, '<', $destdir.OpenBSD::Paths->shells) or return;
	my $_;
	while(<$shells>) {
		s/^\#.*//o;
		return if $_ =~ m/^\Q$fullname\E\s*$/;
	}
	close($shells);
	open(my $shells2, '>>', $destdir.OpenBSD::Paths->shells) or return;
	print $shells2 $fullname, "\n";
	close $shells2;
	$state->say("Shell #1 appended to #2", $fullname,
	    $destdir.OpenBSD::Paths->shells) if $state->verbose;
}

package OpenBSD::PackingElement::Dir;
sub install
{
	my ($self, $state) = @_;
	$self->SUPER::install($state);
	my $fullname = $self->fullname;
	my $destdir = $state->{destdir};

	$state->say("new directory #1", $destdir.$fullname) if $state->verbose >= 5;
	return if $state->{not};
	File::Path::mkpath($destdir.$fullname);
	$self->set_modes($state, $destdir.$fullname);
}

package OpenBSD::PackingElement::Exec;
use OpenBSD::Error;

sub install
{
	my ($self, $state) = @_;

	$self->SUPER::install($state);
	if ($self->should_run($state)) {
		$self->run($state);
	}
}

sub should_run() { 1 }

package OpenBSD::PackingElement::ExecAdd;
sub should_run
{
	my ($self, $state) = @_;
	return !$state->{replacing};
}

package OpenBSD::PackingElement::ExecUpdate;
sub should_run
{
	my ($self, $state) = @_;
	return $state->{replacing};
}

package OpenBSD::PackingElement::Lib;

sub install
{
	my ($self, $state) = @_;
	$self->SUPER::install($state);
	return if $state->{do_faked};
	$self->mark_ldconfig_directory($state->{destdir});
}

package OpenBSD::PackingElement::SpecialFile;
use OpenBSD::PackageInfo;
use OpenBSD::Error;

sub prepare_for_addition
{
	my ($self, $state, $pkgname) = @_;

	my $fname = installed_info($pkgname).$self->name;
	my $cname = $self->fullname;
	my $size = $self->{size};
	if (!defined $size) {
		$size = (stat $cname)[7];
	}
	if ($self->exec_on_add) {
		my $s2 = $state->vstat->stat($cname);
		if (defined $s2 && $s2->noexec) {
			$s2->report_noexec($state, $cname);
		}
	}
	my $s = $state->vstat->add($fname, $self->{size}, $pkgname);
	return unless defined $s;
	if ($s->ro) {
		$s->report_ro($state, $fname);
	}
	if ($s->noexec && $self->exec_on_delete) {
		$s->report_noexec($state, $fname);
	}
	if ($s->avail < 0) {
		$s->report_overflow($state, $fname);
	}
}

sub copy_info
{
	my ($self, $dest) = @_;
	require File::Copy;

	File::Copy::move($self->fullname, $dest) or
		print STDERR "Problem while moving ", $self->fullname,
			" into $dest: $!\n";
}

package OpenBSD::PackingElement::FINSTALL;
sub install
{
	my ($self, $state) = @_;
	$self->run($state, 'PRE-INSTALL');
}

package OpenBSD::PackingElement::FCONTENTS;
sub copy_info
{
}

package OpenBSD::PackingElement::AskUpdate;
sub prepare_for_addition
{
	my ($self, $state, $pkgname, $set) = @_;
	my @old = $set->older_names;
	if ($self->spec->match_ref(\@old) > 0) {
		my $key = "update_".OpenBSD::PackageName::splitstem($pkgname);
		return if $state->defines($key);
		if ($state->{interactive}) {
			if ($state->confirm($self->{message}."\n".
			    "Do you want to update now", 0)) {
			    	return;
			}
		} else {
			$state->errsay("Can't update #1 now: #2",
			    $pkgname, $self->{message});
		}
		$state->{problems}++;
	}
}

1;
