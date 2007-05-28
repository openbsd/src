# ex:ts=8 sw=4:
# $OpenBSD: Delete.pm,v 1.48 2007/05/28 12:16:55 espie Exp $
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
package OpenBSD::Delete;
use OpenBSD::Error;
use OpenBSD::Vstat;
use OpenBSD::PackageInfo;
use OpenBSD::RequiredBy;
use File::Basename;

sub rename_file_to_temp
{
	my $i = shift;
	require OpenBSD::Temp;

	my $n = $i->fullname();

	my ($fh, $j) = OpenBSD::Temp::permanent_file(undef, $n);
	close $fh;
	if (rename($n, $j)) {
		print "Renaming old file $n to $j\n";
		if ($i->{name} !~ m|^/| && $i->cwd() ne '.') {
			my $c = $i->cwd();
			$j =~ s|^\Q$c\E/||;
		}
		$i->{name} = $j;
	} else {
		print "Bad rename $n to $j: $!\n";
	}
}

sub keep_old_files
{
	my ($state, $plist, $dir) = @_;
	my $p = new OpenBSD::PackingList;
	for my $i (qw(cvstags no-default-conflict conflict) ) {
		if (defined $plist->{$i}) {
			$p->{$i} = $plist->{$i};
		}
	}
	for my $i (@{$plist->{items}}) {
		if ($i->isa("OpenBSD::PackingElement::Cwd")) {
			push(@{$p->{items}}, $i);
			next;
		}
		next unless $i->IsFile;
		if (defined $i->{stillaround}) {
			delete $i->{stillaround};
			if ($state->{replacing}) {
				rename_file_to_temp($i);
			}
			push(@{$p->{items}}, $i);
		}
	}
	my $borked = borked_package($plist->pkgname);
	OpenBSD::PackingElement::Name->add($p, $borked);
	$p->{name}->{name} = $borked;
	my $dest = installed_info($borked);
	mkdir($dest);
	require File::Copy;

	File::Copy::copy($dir.COMMENT, $dest);
	File::Copy::copy($dir.DESC, $dest);
	$p->to_installation;
	return $borked;
}

sub manpages_unindex
{
	my ($state) = @_;
	return unless defined $state->{mandirs};
	my $destdir = $state->{destdir};
	require OpenBSD::Makewhatis;

	while (my ($k, $v) = each %{$state->{mandirs}}) {
		my @l = map { $destdir.$_ } @$v;
		if ($state->{not}) {
			print "Removing manpages in $destdir$k: ", join(@l), "\n" if $state->{verbose};
		} else {
			eval { OpenBSD::Makewhatis::remove($destdir.$k, \@l); };
			if ($@) {
				print STDERR "Error in makewhatis: $@\n";
			}
		}
	}
	undef $state->{mandirs};
}

sub validate_plist
{
	my ($plist, $state) = @_;

	$state->{problems} = 0;
	$state->{totsize} = 0;
	$plist->prepare_for_deletion($state, $plist->pkgname);
	Fatal "fatal issues in deinstalling ", $plist->pkgname 
	    if $state->{problems};
	$state->{totsize} = 1 if $state->{totsize} == 0;
	$plist->{totsize} = $state->{totsize};
}

sub remove_packing_info
{
	my $dir = shift;

	for my $fname (info_names()) {
		unlink($dir.$fname);
	}
	OpenBSD::RequiredBy->forget($dir);
	OpenBSD::Requiring->forget($dir);
	rmdir($dir) or Fatal "Can't finish removing directory $dir: $!";
}

sub delete_package
{
	my ($pkgname, $state) = @_;
	OpenBSD::ProgressMeter::message("reading plist");
	my $plist = OpenBSD::PackingList->from_installation($pkgname) or
	    Fatal "Bad package";
	if (!defined $plist->pkgname) {
		Fatal "Package $pkgname has no name";
	}
	if ($plist->pkgname ne $pkgname) {
		Fatal "Package $pkgname real name does not match";
	}

	validate_plist($plist, $state);
	OpenBSD::Vstat::synchronize();

	delete_plist($plist, $state);
}

sub delete_plist
{
	my ($plist, $state) = @_;

	my $totsize = $plist->{totsize};
	my $pkgname = $plist->pkgname;
	$state->{pkgname} = $pkgname;
	my $dir = installed_info($pkgname);
	$state->{dir} = $dir;
	$ENV{'PKG_PREFIX'} = $plist->localbase;
	if ($plist->has(REQUIRE)) {
		$plist->get(REQUIRE)->delete($state);
	}
	if ($plist->has(DEINSTALL)) {
		$plist->get(DEINSTALL)->delete($state);
	} 
	$plist->register_manpage($state);
	manpages_unindex($state);
	my $donesize = 0;
	for my $item (@{$plist->{groups}}, @{$plist->{users}}, @{$plist->{items}}) {
		$item->delete($state);
		if (defined $item->{size}) {
                        $donesize += $item->{size};
                        OpenBSD::ProgressMeter::show($donesize, $totsize);
                }
	}

	OpenBSD::ProgressMeter::next();
	if ($plist->has(UNDISPLAY)) {
		$plist->get(UNDISPLAY)->prepare($state);
	}

	for my $name (OpenBSD::Requiring->new($pkgname)->list) {
		print "remove dependency on $name\n" 
		    if $state->{very_verbose} or $state->{not};
		local $@;
		try { 
			OpenBSD::RequiredBy->new($name)->delete($pkgname);
		} catchall {
			print STDERR "$_\n";
		};
	}
		
	return if $state->{not};
	if ($state->{baddelete}) {
	    my $borked = keep_old_files($state, $plist, $dir);
	    $state->print("Files kept as $borked package\n");
	    delete $state->{baddelete};
	}
			

	remove_packing_info($dir);
}

package OpenBSD::PackingElement;

sub prepare_for_deletion
{
}

sub delete
{
}

sub record_shared
{
}

package OpenBSD::PackingElement::FileObject;
use File::Basename;

sub mark_directory
{
	my ($self, $state, $dir) = @_;

	$state->{dirs_okay}->{$dir} = 1;
	my $d2 = dirname($dir);
	if ($d2 ne $dir) {
		$self->mark_directory($state, $d2);
	}
}

sub mark_dir
{
	my ($self, $state) = @_;

	$self->mark_directory($state, dirname($self->fullname()));
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

sub do_not_delete
{
	my ($self, $state) = @_;

	my $realname = $self->realname($state);
	$state->{baddelete} = 1;
	$self->{stillaround} = 1;

	delete $self->{symlink};
	delete $self->{link};
	delete $self->{md5};

	if (-l $realname) {
		$self->{symlink} = readlink $realname;
	} elsif (-f _) {
		$self->{md5} = OpenBSD::md5::fromfile($realname);
	} elsif (-d _) {
		# what should we do ?
	}
}


package OpenBSD::PackingElement::DirlikeObject;
sub mark_dir
{
	my ($self, $state) = @_;
	$self->mark_directory($state, $self->fullname);
}

package OpenBSD::PackingElement::NewUser;
sub delete
{
	my ($self, $state) = @_;

	if ($state->{beverbose}) {
		print "rmuser: $self->{name}\n";
	}

	$self->record_shared($state->{recorder}, $state->{pkgname});
}

sub record_shared
{
	my ($self, $recorder, $pkgname) = @_;
	$recorder->{users}->{$self->{name}} = $pkgname;
}

package OpenBSD::PackingElement::NewGroup;
sub delete
{
	my ($self, $state) = @_;

	if ($state->{beverbose}) {
		print "rmgroup: $self->{name}\n";
	}

	$self->record_shared($state->{recorder}, $state->{pkgname});
}

sub record_shared
{
	my ($self, $recorder, $pkgname) = @_;
	$recorder->{groups}->{$self->{name}} = $pkgname;
}

package OpenBSD::PackingElement::DirBase;
sub delete
{
	my ($self, $state) = @_;

	if ($state->{very_verbose}) {
		print "dirrm: ", $self->fullname, "\n";
	}

	$self->record_shared($state->{recorder}, $state->{pkgname});
}

sub record_shared
{
	my ($self, $recorder, $pkgname) = @_;
	$self->{pkgname} = $pkgname;
	push(@{$recorder->{dirs}->{$self->fullname}} , $self);
}

package OpenBSD::PackingElement::DirRm;
sub delete
{
	&OpenBSD::PackingElement::DirBase::delete;
}

package OpenBSD::PackingElement::Unexec;
sub delete
{
	my ($self, $state) = @_;
	$self->run($state);
}

package OpenBSD::PackingElement::FileBase;
use OpenBSD::md5;
use OpenBSD::Error;
use OpenBSD::Vstat;

sub prepare_for_deletion
{
	my ($self, $state, $pkgname) = @_;

	my $fname = $state->{destdir}.$self->fullname;
	$state->{totsize} += $self->{size} if defined $self->{size};
	my $s = OpenBSD::Vstat::remove($fname, $self->{size});
	return unless defined $s;
	if ($s->{ro}) {
		$s->report_ro($state, $fname);
	}
}

sub delete
{
	my ($self, $state) = @_;
	my $realname = $self->realname($state);

	if (defined $self->{symlink}) {
		if (-l $realname) {
			my $contents = readlink $realname;
			if ($contents ne $self->{symlink}) {
				print "Symlink does not match: $realname ($contents vs. ", $self->{symlink},")\n";
				$self->do_not_delete($state);
				return;
			}
		} else  {
			print "Bogus symlink: $realname\n";
			$self->do_not_delete($state);
			return;
		}
	} else {
		if (-l $realname) {
				print "Unexpected symlink: $realname\n";
				$self->do_not_delete($state);
		} else {
			if (! -f $realname) {
				print "File $realname does not exist\n";
				return;
			}
			unless (defined($self->{link}) or $self->{nochecksum} or $state->{quick}) {
				if (!defined $self->{md5}) {
					print "Problem: ", $self->fullname(),
					    " does not have an md5 checksum\n";
					print "NOT deleting: $realname\n";
					$state->print("Couldn't delete $realname (no md5)\n");
					return;
				}
				my $md5 = OpenBSD::md5::fromfile($realname);
				if ($md5 ne $self->{md5}) {
					print "Problem: md5 doesn't match for ",
						$self->fullname, "\n";
					print "NOT deleting: $realname\n";
					$state->print("Couldn't delete $realname (bad md5)\n");
					$self->do_not_delete($state);
					return;
				}
			}
		}
	}
	if ($state->{very_verbose}) {
		print "deleting: $realname\n";
	}
	return if $state->{not};
	if (!unlink $realname) {
		print "Problem deleting $realname\n";
		$state->print("deleting $realname failed: $!\n");
	}
}

package OpenBSD::PackingElement::SpecialFile;
use OpenBSD::PackageInfo;

sub prepare_for_deletetion
{
	my ($self, $state, $pkgname) = @_;

	my $fname = installed_info($pkgname).$self->{name};
	my $size = $self->{size};
	if (!defined $size) {
		$size = (stat $fname)[7];
	}
	my $s = OpenBSD::Vstat::remove($fname, $self->{size});
	return unless defined $s;
	if ($s->{ro}) {
		$s->report_ro($state, $fname);
	}
	if ($s->{noexec} && $self->exec_on_delete) {
		$s->report_noexec($state, $fname);
	}
}

package OpenBSD::PackingElement::Sample;
use OpenBSD::md5;
use OpenBSD::Error;
use File::Basename;

sub delete
{
	my ($self, $state) = @_;
	my $realname = $self->realname($state);

	my $orig = $self->{copyfrom};
	if (!defined $orig) {
		Fatal "\@sample element does not reference a valid file\n";
	}
	my $origname = $orig->realname($state);
	if (! -e $realname) {
		$state->print("File $realname does not exist\n");
		return;
	}
	if (! -f $realname) {
		$state->print("File $realname is not a file\n");
		return;
	}

	if (!defined $orig->{md5}) {
		$state->print("Couldn't delete $realname (no md5)\n");
		return;
	}

	if ($state->{quick}) {
		unless ($state->{extra}) {
			$self->mark_dir($state);
			$state->print("You should also remove $realname\n");
			return;
		}
	} else {
		my $md5 = OpenBSD::md5::fromfile($realname);
		if ($md5 eq $orig->{md5}) {
			print "File $realname identical to sample\n" if $state->{not} or $state->{verbose};
		} else {
			unless ($state->{extra}) {
				$self->mark_dir($state);
				$state->print("You should also remove $realname (which was modified)\n");
				return;
			}
		}
	}
	return if $state->{not};
	print "deleting $realname\n" if $state->{verbose};
	if (!unlink $realname) {
		print "Problem deleting $realname\n";
		$state->print("deleting $realname failed: $!\n");
	}
}
		

package OpenBSD::PackingElement::InfoFile;
use File::Basename;
use OpenBSD::Error;
sub delete
{
	my ($self, $state) = @_;
	unless ($state->{not}) {
	    my $fullname = $state->{destdir}.$self->fullname();
	    VSystem($state->{very_verbose}, 
	    "install-info", "--delete", "--info-dir=".dirname($fullname), $fullname);
	}
	$self->SUPER::delete($state);
}

package OpenBSD::PackingElement::Shell;
sub delete
{
	my ($self, $state) = @_;
	unless ($state->{not}) {
		my $destdir = $state->{destdir};
		my $fullname = $self->fullname;
		my @l=();
		if (open(my $shells, '<', $destdir.'/etc/shells')) {
			local $_;
			while(<$shells>) {
				push(@l, $_);
				s/^\#.*//;
				if ($_ =~ m/^\Q$fullname\E\s*$/) {
					pop(@l);
				}
			}
			close($shells);
			open(my $shells2, '>', $destdir.'/etc/shells');
			print $shells2 @l;
			close $shells2;
			print "Shell $fullname removed from $destdir/etc/shells\n";
		}
	}
	$self->SUPER::delete($state);
}

package OpenBSD::PackingElement::Extra;
use File::Basename;

sub delete
{
	my ($self, $state) = @_;
	my $realname = $self->realname($state);
	if ($state->{beverbose} && $state->{extra}) {
		print "deleting extra file: $realname\n";
	}
	return if $state->{not};
	return unless -e $realname or -l $realname;
	if ($state->{replacing}) {
		$state->print("Remember to update $realname\n");
		$self->mark_dir($state);
	} elsif ($state->{extra}) {
		unlink($realname) or 
		    print "problem deleting extra file $realname\n";
	} else {
		$state->print("You should also remove $realname\n");
		$self->mark_dir($state);
	}
}


package OpenBSD::PackingElement::Extradir;
sub delete
{
	my ($self, $state) = @_;
	return unless $state->{extra};
	my $realname = $self->realname($state);
	return if $state->{replacing};
	if ($state->{extra}) {
		$self->SUPER::delete($state);
	} else {
		$state->print("You should also remove the directory $realname\n");
		$self->mark_dir($state);
	}
}

package OpenBSD::PackingElement::ExtraUnexec;

sub delete
{
	my ($self, $state) = @_;
	if ($state->{extra}) {
		$self->run($state);
	} else {
		$state->print("You should also run ", $self->{expanded}, "\n");
	}
}

package OpenBSD::PackingElement::Lib;
sub delete
{
	my ($self, $state) = @_;
	$self->SUPER::delete($state);
	$self->mark_ldconfig_directory($state->{destdir});
}

package OpenBSD::PackingElement::FREQUIRE;
sub delete
{
	my ($self, $state) = @_;

	$self->run($state, "DEINSTALL");
}

package OpenBSD::PackingElement::FDEINSTALL;
sub delete
{
	my ($self, $state) = @_;

	$self->run($state, "DEINSTALL");
}

1;
