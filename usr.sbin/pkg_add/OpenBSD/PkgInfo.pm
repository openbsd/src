#! /usr/bin/perl
# ex:ts=8 sw=4:
# $OpenBSD: PkgInfo.pm,v 1.3 2010/06/09 10:25:17 espie Exp $
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

package OpenBSD::PackingElement;
sub dump_file
{
}

sub hunt_file
{
}

sub sum_up
{
	my ($self, $rsize) = @_;
	if (defined $self->{size}) {
		$$rsize += $self->{size};
	}
}

package OpenBSD::PackingElement::FileBase;
sub dump_file
{
	my ($item, $opt_K) = @_;
	if ($opt_K) {
		print '@', $item->keyword, " ";
	}
	print $item->fullname, "\n";
}

package OpenBSD::PackingElement::FileObject;
sub hunt_file
{
	my ($item, $h, $pkgname, $l) = @_;
	my $fname = $item->fullname;
	if (defined $h->{$fname}) {
		push(@{$h->{$fname}}, $pkgname);
		push(@$l, $pkgname);
	}
}

package OpenBSD::PkgInfo;
use OpenBSD::PackageInfo;
use OpenBSD::PackageName;
use OpenBSD::Getopt;
use OpenBSD::Error;
use OpenBSD::State;


my $total_size = 0;
my $pkgs = 0;

sub find_pkg_in
{
	my ($repo, $pkgname, $code) = @_;

	if (OpenBSD::PackageName::is_stem($pkgname)) {
		require OpenBSD::Search;
		my $l = $repo->match_locations(OpenBSD::Search::Stem->new($pkgname));
		if (@$l != 0) {
			for my $pkg (sort {$a->name cmp $b->name} @$l) {
				&$code($pkg->name, $pkg);
				$pkg->close_now;
				$pkg->wipe_info;
			}
			return 1;
		}
	}
	# okay, so we're actually a spec in disguise
	if ($pkgname =~ m/[\*\<\>\=]/) {
		require OpenBSD::Search;
		my $s = OpenBSD::Search::PkgSpec->new($pkgname);
		if (!$s->is_valid) {
			print STDERR "Invalid spec: $pkgname\n";
			return 0;
		}
		my $r = $repo->match_locations($s);
		if (@$r == 0) {
			return 0;
		} else {
			for my $pkg (@$r) {
				&$code($pkgname, $pkg);
				$pkg->close_now;
				$pkg->wipe_info;
			}
			return 1;
		}
	} else {
		my $pkg = $repo->find($pkgname);
		if (defined $pkg) {
			&$code($pkgname, $pkg);
			$pkg->close_now;
			$pkg->wipe_info;
			return 1;
		}
		return 0;
	}
}

sub find_pkg
{
	my ($pkgname, $code) = @_;
	require OpenBSD::PackageRepository::Installed;

	if (find_pkg_in(OpenBSD::PackageRepository::Installed->new, $pkgname,
	    $code)) {
		return;
	}
	require OpenBSD::PackageLocator;

	my $repo;

	if ($pkgname =~ m/[\/\:]/o) {
		($repo, $pkgname) =
		    OpenBSD::PackageLocator->path_parse($pkgname);
	} else {
		$repo = 'OpenBSD::PackageLocator';
	}

	find_pkg_in($repo, $pkgname, $code);
}

sub printfile
{
	my $filename = shift;
	my $_;

	open my $fh, '<', $filename or return;
	while(<$fh>) {
		print;
	}
	close $fh;
}

sub print_description
{
	my $dir = shift;
	my $_;

	open my $fh, '<', $dir.DESC or return;
	$_ = <$fh> unless -f $dir.COMMENT;
	while(<$fh>) {
		print;
	}
	close $fh;
}

sub get_line
{
	open my $fh, '<', shift or return "";
	my $c = <$fh>;
	chomp($c);
	close $fh;
	return $c;
}

sub get_comment
{
	my $d = shift;
	return get_line(-f $d.COMMENT? $d.COMMENT : $d.DESC);
}

sub find_by_spec
{
	my $pat = shift;

	require OpenBSD::Search;
	require OpenBSD::PackageRepository::Installed;

	my $s = OpenBSD::Search::PkgSpec->new($pat);
	if (!$s->is_valid) {
		print STDERR "Invalid spec: $pat\n";
		return ();
	} else {
		my $r = OpenBSD::PackageRepository::Installed->new->match_locations($s);

		return sort (map {$_->name} @$r);
	}
}

sub filter_files
{
	require OpenBSD::PackingList;

	my $search = shift;
	my @result = ();
	for my $arg (@_) {
		find_pkg($arg,
		    sub {
		    	my ($pkgname, $handle) = @_;

			my $plist = $handle->plist(\&OpenBSD::PackingList::FilesOnly);

			$plist->hunt_file($search, $pkgname, \@result);
		    });
	}
	return @result;
}

sub manual_filter
{
	require OpenBSD::PackingList;

	my @result = ();
	for my $arg (@_) {
		find_pkg($arg,
		    sub {
		    	my ($pkgname, $handle) = @_;

			my $plist = $handle->plist(\&OpenBSD::PackingList::ConflictOnly);

			push(@result, $pkgname) if $plist->has('manual-installation');
		    });
	}
	return @result;
}

my $path_info;

sub add_to_path_info
{
	my ($path, $pkgname) = @_;

	$path_info->{$path} = [] unless
	    defined $path_info->{$path};
	push(@{$path_info->{$path}}, $pkgname);
}

sub find_by_path
{
	my $pat = shift;

	if (!defined $path_info) {
		require OpenBSD::PackingList;

		$path_info = {};
		for my $pkg (installed_packages(1)) {
			my $plist =
				OpenBSD::PackingList->from_installation($pkg,
				    \&OpenBSD::PackingList::ExtraInfoOnly);
			next if !defined $plist;
			if (defined $plist->fullpkgpath) {
				add_to_path_info($plist->fullpkgpath,
				    $plist->pkgname);
			}
			if ($plist->has('pkgpath')) {
				for my $p (@{$plist->{pkgpath}}) {
					add_to_path_info($p->name,
					    $plist->pkgname);
				}
			}
		}
	}
	if (defined $path_info->{$pat}) {
		return @{$path_info->{$pat}};
	} else {
		return ();
	}
}

our ($opt_c, $opt_C, $opt_d, $opt_f, $opt_I, $opt_K,
    $opt_L, $opt_Q, $opt_q, $opt_R, $opt_s, $opt_v, $opt_h,
    $opt_l, $opt_a, $opt_m, $opt_M, $opt_U, $opt_A, $opt_S, $opt_P, $opt_t);
my $terse = 0;
my $exit_code = 0;
my $error_e = 0;
my @sought_files;
my $state;

sub just_in_time_header
{
	my ($pkg, $handle, $rdone) = @_;
	return if $$rdone == 1;
	if (!$terse && !$opt_q) {
		print $opt_l, "Information for ", $handle->url, "\n\n";
	}
	$$rdone = 1;
}

sub print_info
{
	my ($pkg, $handle) = @_;
	unless (defined $handle) {
		print STDERR "Error printing info for $pkg: no info ?\n";
	}
	if ($opt_I) {
		my $l = 20 - length($pkg);
		$l = 1 if $l <= 0;
		print $pkg;
		print " "x$l, get_comment($handle->info) unless $opt_q;
		print "\n";
	} else {
		my $done = 0;
		if ($opt_c) {
			just_in_time_header($pkg, $handle ,\$done);
			print $opt_l, "Comment:\n" unless $opt_q;
			print get_comment($handle->info), "\n";
			print "\n";
		}
		if ($opt_R && -f $handle->info.REQUIRED_BY) {
			just_in_time_header($pkg, $handle ,\$done);
			print $opt_l, "Required by:\n" unless $opt_q;
			printfile($handle->info.REQUIRED_BY);
			print "\n";
		}
		if ($opt_d) {
			just_in_time_header($pkg, $handle ,\$done);
			print $opt_l, "Description:\n" unless $opt_q;
			print_description($handle->info);
			print "\n";
		}
		if ($opt_M && -f $handle->info.DISPLAY) {
			just_in_time_header($pkg, $handle ,\$done);
			print $opt_l, "Install notice:\n" unless $opt_q;
			printfile($handle->info.DISPLAY);
			print "\n";
		}
		if ($opt_U && -f $handle->info.UNDISPLAY) {
			just_in_time_header($pkg, $handle ,\$done);
			print $opt_l, "Deinstall notice:\n" unless $opt_q;
			printfile($handle->info.UNDISPLAY);
			print "\n";
		}
		my $plist;
		if ($opt_f || $opt_L || $opt_s || $opt_S || $opt_C) {
			require OpenBSD::PackingList;

			if ($opt_f || $opt_s || $opt_S || $opt_C) {
				$plist = $handle->plist;
			} else {
				$plist = $handle->plist(\&OpenBSD::PackingList::FilesOnly);
			}
			$state->fatal("bad packing-list for", $handle->url)
			    unless defined $plist;
		}
		if ($opt_L) {
			just_in_time_header($pkg, $handle ,\$done);
			print $opt_l, "Files:\n" unless $opt_q;
			$plist->dump_file($opt_K);
			print "\n";
		}
		if ($opt_C) {
			just_in_time_header($pkg, $handle ,\$done);
			if ($plist->is_signed) {

				require OpenBSD::x509;
				print $opt_l, "Certificate info:\n" unless $opt_q;
				OpenBSD::x509::print_certificate_info($plist);
			} else {
				print $opt_l, "No digital signature\n" unless $opt_q;
			}
		}
		if ($opt_s) {
			just_in_time_header($pkg, $handle ,\$done);
			my $size = 0;
			$plist->sum_up(\$size);
			print "Size: " unless $opt_q;
			print "$size\n";
			$total_size += $size;
			$pkgs++;
		}
		if ($opt_S) {
			just_in_time_header($pkg, $handle ,\$done);
			print "Signature: " unless $opt_q;
			print $plist->signature->string, "\n";
		}
		if ($opt_P) {
			require OpenBSD::PackingList;

			my $plist = $handle->plist(
			    \&OpenBSD::PackingList::ExtraInfoOnly);
			just_in_time_header($pkg, $handle ,\$done);
			print "Pkgpath:\n" unless $opt_q;
			if (defined $plist->fullpkgpath) {
				print $plist->fullpkgpath;
			} else {
				print STDERR $plist->pkgname,
				    " has no FULLPKGPATH\n";
			}
			print "\n";
		}

		if ($opt_f) {
			just_in_time_header($pkg, $handle ,\$done);
			print $opt_l, "Packing-list:\n" unless $opt_q;
			$plist->write(\*STDOUT);
			print "\n";
		}
		return unless $done;
		print $opt_l, "\n" unless $opt_q || $terse;
	}
}

sub parse_and_run
{
	my ($self, $cmd) = @_;
	$state = OpenBSD::State->new($cmd);
	$state->usage_is('[-AaCcdfIKLMmPqRSstUv] [-E filename] [-e pkg-name] [-l str] [-Q query] [pkg-name] [...]');

	my %defines;
	my $locked;
	try {
		getopts('cCdfF:hIKLmPQ:qRsSUve:E:Ml:aAt',
		    {'e' =>
			    sub {
				    my $pat = shift;
				    my @list;
				    lock_db(1, $opt_q) unless $defines{nolock};
				    $locked = 1;
				    if ($pat =~ m/\//o) {
					    @list = find_by_path($pat);
				    } else {
					    @list = find_by_spec($pat);
				    }
				    if (@list == 0) {
					    $exit_code = 1;
					    $error_e = 1;
				    }
				    push(@ARGV, @list);
				    $terse = 1;
			    },
		     'F' => sub {
				    for my $o (split /\,/o, shift) {
					    $defines{$o} = 1;
				    }
			    },
		     'h' => sub {	$state->usage; },
		     'E' =>
			    sub {
				    require File::Spec;

				    push(@sought_files, File::Spec->rel2abs(shift));

			    }
		});
	} catchall {
		$state->usage("#1", $_);
	};

	lock_db(1, $opt_q) unless $locked or $defines{nolock};

	unless ($opt_c || $opt_M || $opt_U || $opt_d || $opt_f || $opt_I ||
		$opt_L || $opt_R || $opt_s ||
		$opt_S || $opt_P || $terse) {
		if (@ARGV == 0) {
			$opt_I = $opt_a = 1;
		} else {
			$opt_c = $opt_d = $opt_M = $opt_R = 1;
		}
	}

	if ($opt_Q) {
		require OpenBSD::PackageLocator;
		require OpenBSD::Search;

		print "PKG_PATH=$ENV{PKG_PATH}\n" if $opt_v;
		my $partial = OpenBSD::Search::PartialStem->new($opt_Q);

		my $r = OpenBSD::PackageLocator->match_locations($partial);

		for my $p (sort map {$_->name} @$r) {
			print $p, is_installed($p) ? " (installed)" : "" , "\n";
		}

		exit 0;
	}

	if ($opt_v) {
		$opt_c = $opt_d = $opt_f = $opt_M =
		    $opt_U = $opt_R = $opt_s = $opt_S = 1;
	}

	if (!defined $opt_l) {
		$opt_l = "";
	}

	if ($opt_K && !$opt_L) {
		$state->usage("-K only makes sense with -L");
	}

	if (@ARGV == 0 && !$opt_a && !$opt_A) {
		$state->usage("Missing package name(s)") unless $terse || $opt_q;
	}

	if (@ARGV > 0 && ($opt_a || $opt_A)) {
		$state->usage("Can't specify package name(s) with -a");
	}

	if (@ARGV > 0 && $opt_t) {
		$state->usage("Can't specify package name(s) with -t");
	}

	if (@ARGV > 0 && $opt_m) {
		$state->usage("Can't specify package name(s) with -m");
	}

	if (@ARGV == 0 && !$error_e) {
		@ARGV = sort(installed_packages(defined $opt_A ? 0 : 1));
		if ($opt_t) {
			require OpenBSD::RequiredBy;
			@ARGV = grep { OpenBSD::RequiredBy->new($_)->list == 0 } @ARGV;
		}
	}

	if (@sought_files) {
		my %hash = map { ($_, []) }  @sought_files;
		@ARGV = filter_files(\%hash, @ARGV);
		for my $f (@sought_files) {
			my $l = $hash{$f};
			if (@$l) {
				print "$f: ", join(',', @$l), "\n" unless $opt_q;
			} else {
				$exit_code = 1;
			}
		}
	}

	if ($opt_m) {
		@ARGV = manual_filter(@ARGV);
	}

	for my $pkg (@ARGV) {
		if ($terse && !$opt_q) {
			print $opt_l, $pkg, "\n";
		}
		find_pkg($pkg, \&print_info);
	}
	if ($pkgs > 1) {
		print "Total size: $total_size\n";
	}
	exit($exit_code);
}
1;
