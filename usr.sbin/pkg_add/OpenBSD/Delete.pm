# ex:ts=8 sw=4:
# $OpenBSD: Delete.pm,v 1.11 2004/11/21 13:32:18 espie Exp $
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
package OpenBSD::Delete;
use OpenBSD::Error;
use OpenBSD::Vstat;
use OpenBSD::PackageInfo;

sub manpages_unindex
{
	my ($state) = @_;
	return unless defined $state->{mandirs};
	my $destdir = $state->{destdir};
	require OpenBSD::Makewhatis;

	while (my ($k, $v) = each %{$state->{mandirs}}) {
		my @l = map { $destdir.$_ } @$v;
		if ($state->{not}) {
			print "Removing manpages in $destdir$k: ", join(@l), "\n";
		} else {
			eval { OpenBSD::Makewhatis::remove($destdir.$k, \@l); };
			if ($@) {
				print STDERR "Error in makewhatis: $@\n";
			}
		}
	}
	undef $state->{mandirs};
}

sub validate_plist($$)
{
	my ($plist, $state) = @_;

	my $destdir = $state->{destdir};
	my $problems = 0;
	my $totsize = 0;
	for my $item (@{$plist->{items}}) {
		next unless $item->IsFile();
		my $fname = $destdir.$item->fullname();
		$totsize += $item->{size} if defined $item->{size};
		my $s = OpenBSD::Vstat::remove($fname, $item->{size});
		next unless defined $s;
		if ($s->{ro}) {
			Warn "Error: ", $s->{mnt}, " is read-only ($fname)\n";
			$problems++;
		}
	}
	Fatal "fatal issues" if $problems;
	$plist->{totsize} = $totsize;
}

sub remove_packing_info
{
	my $dir = shift;

	for my $fname (info_names()) {
		unlink($dir.$fname);
	}
	rmdir($dir) or Fatal "Can't finish removing directory $dir: $!";
}

sub delete_package
{
	my ($pkgname, $state) = @_;
	my $plist = OpenBSD::PackingList->from_installation($pkgname) or
	    Fatal "Bad package";
	if (!defined $plist->pkgname()) {
		Fatal "Package $pkgname has no name";
	}
	if ($plist->pkgname() ne $pkgname) {
		Fatal "Package $pkgname real name does not match";
	}

	validate_plist($plist, $state);

	delete_plist($plist, $state);
}

sub delete_plist
{
	my ($plist, $state) = @_;

	my $totsize = $plist->{totsize};
	my $pkgname = $plist->pkgname();
	$state->{pkgname} = $pkgname;
	my $dir = installed_info($pkgname);
	$state->{dir} = $dir;
	$ENV{'PKG_PREFIX'} = $plist->pkgbase();
	if ($plist->has(REQUIRE)) {
		$plist->get(REQUIRE)->delete($state);
	}
	if ($plist->has(DEINSTALL)) {
		$plist->get(DEINSTALL)->delete($state);
	} 
	$plist->visit('register_manpage', $state);
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

	# guard against duplicate pkgdep
	my $removed = {};

	my $zap_dependency = sub {
		my $name = shift;

		return if defined $removed->{$name};
		print "remove dependency on $name\n" 
		    if $state->{very_verbose} or $state->{not};
		local $@;
		eval { OpenBSD::RequiredBy->new($name)->delete($pkgname) unless $state->{not}; };
		if ($@) {
			print STDERR "$@\n";
		}
		$removed->{$name} = 1;
	};

	for my $item (@{$plist->{pkgdep}}) {
		&$zap_dependency($item->{name});
	}
	for my $name (OpenBSD::Requiring->new($pkgname)->list()) {
		&$zap_dependency($name);
	}
		
	remove_packing_info($dir) unless $state->{not};
	$plist->forget();
}

package OpenBSD::PackingElement;

sub delete
{
}

package OpenBSD::PackingElement::NewUser;
sub delete
{
	my ($self, $state) = @_;

	my $name = $self->{name};

	if ($state->{beverbose}) {
		print "rmuser: $name\n";
	}

	$state->{users_to_rm} = {} unless defined $state->{users_to_rm};

	my $h = $state->{users_to_rm};
	$h->{$name} = $state->{pkgname};
}

package OpenBSD::PackingElement::NewGroup;
sub delete
{
	my ($self, $state) = @_;

	my $name = $self->{name};

	if ($state->{beverbose}) {
		print "rmgroup: $name\n";
	}

	$state->{groups_to_rm} = {} unless defined $state->{groups_to_rm};

	my $h = $state->{groups_to_rm};
	$h->{$name} = 1;
}

package OpenBSD::PackingElement::DirBase;
sub delete
{
	my ($self, $state) = @_;

	my $name = $self->fullname();

	if ($state->{beverbose}) {
		print "dirrm: $name\n";
	}

	$state->{dirs_to_rm} = {} unless defined $state->{dirs_to_rm};

	my $h = $state->{dirs_to_rm};
	$h->{$name} = [] unless defined $h->{$name};
	$self->{pkgname} = $state->{pkgname};
	push(@{$h->{$name}}, $self);
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
sub delete
{
	my ($self, $state) = @_;
	my $name = $self->fullname();
	if (defined $self->{tempname}) {
		$name = $self->{tempname};
	}
	my $realname = $state->{destdir}.$name;
	if (-l $realname) {
		if ($state->{beverbose}) {
			print "deleting symlink: $realname\n";
		}
	} else {
		if (! -f $realname) {
			print "File $realname does not exist\n";
			return;
		}
		unless (defined($self->{link}) or $self->{nochecksum} or $state->{quick}) {
			if (!defined $self->{md5}) {
				print "Problem: $name does not have an md5 checksum\n";
				print "NOT deleting: $realname\n";
				$state->print("Couldn't delete $realname (no md5)\n");
				return;
			}
			my $md5 = OpenBSD::md5::fromfile($realname);
			if ($md5 ne $self->{md5}) {
				print "Problem: md5 doesn't match for $name\n";
				print "NOT deleting: $realname\n";
				$state->print("Couldn't delete $realname (bad md5)\n");
				return;
			}
		}
		if ($state->{beverbose}) {
			print "deleting: $realname\n";
		}
	}
	return if $state->{not};
	if (!unlink $realname) {
		print "Problem deleting $realname\n";
		$state->print("deleting $realname failed: $!\n");
	}
}

package OpenBSD::PackingElement::Sample;
use OpenBSD::md5;
use OpenBSD::Error;
sub delete
{
	my ($self, $state) = @_;
	my $name = $self->fullname();
	my $realname = $state->{destdir}.$name;

	my $orig = $self->{copyfrom};
	if (!defined $orig) {
		Fatal "\@sample element does not reference a valid file\n";
	}
	my $origname = $state->{destdir}.$orig->fullname();
	if (! -e $realname) {
		print "File $realname does not exist\n";
		return;
	}
	if (! -f $realname) {
		print "File $realname is not a file\n";
		return;
	}
	if (!defined $orig->{md5}) {
		print "Problem: file $name does not have an md5 checksum\n";
		print "NOT deleting: $realname\n";
		$state->print("Couldn't delete $realname (no md5)\n");
		return;
	}

	if ($state->{quick}) {
		unless ($state->{extra}) {
			print "NOT deleting file $realname\n";
			return;
		}
	} else {
		my $md5 = OpenBSD::md5::fromfile($realname);
		if ($md5 eq $orig->{md5}) {
			print "File $realname identical to sample\n" if $state->{not} or $state->{verbose};
		} else {
			print "File $realname NOT identical to sample\n";
			unless ($state->{extra}) {
				print "NOT deleting $realname\n";
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
		my $fullname = $self->fullname();
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
sub delete
{
	my ($self, $state) = @_;
	return unless $state->{extra};
	my $name = $self->fullname();
	my $realname = $state->{destdir}.$name;
	if ($state->{beverbose}) {
		print "deleting extra file: $realname\n";
	}
	return if $state->{not};
	return unless -e $realname;
	unlink($realname) or 
	    print "problem deleting extra file $realname\n";
}

package OpenBSD::PackingElement::Extradir;
sub delete
{
	my ($self, $state) = @_;
	return unless $state->{extra};
	return unless -e $state->{destdir}.$self->fullname();
	$self->SUPER::delete($state);
}

package OpenBSD::PackingElement::ExtraUnexec;

sub delete
{
	my ($self, $state) = @_;
	return unless $state->{extra};

	$self->run($state);
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
