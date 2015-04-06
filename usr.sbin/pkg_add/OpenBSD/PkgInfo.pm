#! /usr/bin/perl
# ex:ts=8 sw=4:
# $OpenBSD: PkgInfo.pm,v 1.34 2015/04/06 11:14:58 espie Exp $
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

use OpenBSD::State;

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

package OpenBSD::PkgInfo::State;
our @ISA = qw(OpenBSD::State);

use OpenBSD::PackageInfo;

sub lock
{
	my $state = shift;
	return if $state->{locked};
	return if $state->{subst}->value('nolock');
	lock_db(1, $state->opt('q') ? undef : $state);
	$state->{locked} = 1;
}

sub banner
{
	my ($state, @args) = @_;
	return if $state->opt('q');
	$state->print("#1", $state->opt('l')) if $state->opt('l');
	$state->say(@args);
}

sub header
{
	my ($state, $handle) = @_;
	return if $state->{terse} || $state->opt('q');
	my $url = $handle->url;
	return if $state->{header_done}{$url};
	$state->{header_done}{$url} = 1;
	$state->banner("Information for #1\n", $url);
}

sub footer
{
	my ($state, $handle) = @_;
	return if $state->opt('q') || $state->{terse};
	return unless $state->{header_done}{$handle->url};
	if ($state->opt('l')) {
		$state->say("#1", $state->opt('l'));
	} else {
		$state->say;
	}
}

sub printfile
{
	my ($state, $filename) = @_;

	open my $fh, '<', $filename or return;
	while(<$fh>) {
		chomp;
		$state->say("#1", $_);
	}
	close $fh;
	$state->say;
}

sub printfile_sorted
{
	my ($state, $filename) = @_;

	open my $fh, '<', $filename or return;
	my @lines = (<$fh>);
	close $fh;
	foreach my $line (sort @lines) {
		chomp $line;
		$state->say("#1", $line);
	}
	$state->say;
}

sub print_description
{
	my ($state, $dir) = @_;

	open my $fh, '<', $dir.DESC or return;
	$_ = <$fh>; # zap COMMENT
	while(<$fh>) {
		chomp;
		$state->say("#1", $_);
	}
	close $fh;
	$state->say;
}

sub hasanyopt
{
	my ($self, $string) = @_;
	for my $i (split //, $string) {
		if ($self->opt($i)) {
			return 1;
		}
	}
	return 0;
}

sub setopts
{
	my ($self, $string) = @_;
	for my $i (split //, $string) {
		$self->{opt}{$i} = 1;
	}
}

package OpenBSD::PkgInfo;
use OpenBSD::PackageInfo;
use OpenBSD::PackageName;
use OpenBSD::Getopt;
use OpenBSD::Error;


my $total_size = 0;
my $pkgs = 0;

sub find_pkg_in
{
	my ($self, $state, $repo, $pkgname, $code) = @_;

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
			$state->errsay("Invalid spec: #1", $pkgname);
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
	my ($self, $state, $pkgname, $code) = @_;

	if ($self->find_pkg_in($state, $state->repo->installed, $pkgname,
	    $code)) {
		return;
	}
	my $repo;

	if ($pkgname =~ m/[\/\:]/o) {
		($repo, $pkgname) = $state->repo->path_parse($pkgname);
	} else {
		$repo = $state->repo;
	}

	$self->find_pkg_in($state, $repo, $pkgname, $code);
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
	return get_line($d.DESC);
}

sub find_by_spec
{
	my ($pat, $state) = @_;

	require OpenBSD::Search;

	my $s = OpenBSD::Search::PkgSpec->new($pat);
	if (!$s->is_valid) {
		$state->errsay("Invalid spec: #1", $pat);
		return ();
	} else {
		my $r = $state->repo->installed->match_locations($s);

		return sort {$a->name cmp $b->name} @$r;
	}
}

sub filter_files
{
	my ($self, $state, $search, @args) = @_;
	require OpenBSD::PackingList;

	my @k = ();
	for my $file (keys %$search) {
		my $k = $file;
		if ($file =~ m|^.*/(.*?)$|) {
			$k = $1;
		}
		push(@k, quotemeta($k));
	}
	my $re = join('|', @k);

	my @result = ();
	for my $arg (@args) {
		$self->find_pkg($state, $arg,
		    sub {
		    	my ($pkgname, $handle) = @_;

			if (-f $handle->info.CONTENTS) {
				my $maybe = 0;
				open(my $fh, '<', $handle->info.CONTENTS);
				while (<$fh>) {
					if (m/$re/) {
						$maybe = 1;
						last;
					}
				}
				close($fh);
				return if !$maybe;
			}
			my $plist = $handle->plist(\&OpenBSD::PackingList::FilesOnly);

			$plist->hunt_file($search, $pkgname, \@result);
		    });
	}
	return @result;
}

sub manual_filter
{
	my ($self, $state, @args) = @_;
	require OpenBSD::PackingList;

	my @result = ();
	for my $arg (@args) {
		$self->find_pkg($state, $arg,
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

sub may_check_data
{
	my ($self, $handle, $pkgname, $state, $r) = @_;
	# don't check installed packages
	return if  $handle->trusted;
	require OpenBSD::PackingList;
	$$r //= $handle->plist;
	if ($$r->is_signed) {
		if ($state->defines('nosig')) {
			$state->errsay("NOT CHECKING DIGITAL SIGNATURE FOR #1",
			    $pkgname);
		} else {
			if (!$$r->check_signature($state)) {
				$state->fatal("#1 is corrupted", $pkgname);
			}
		}
	}
	for my $name (OpenBSD::PackageInfo::info_names()) {
		if ($$r->has($name)) {
			$$r->get($name)->may_verify_digest($state);
		}
	}
}

sub print_info
{
	my ($self, $state, $pkg, $handle) = @_;
	unless (defined $handle) {
		$state->errsay("Error printing info for #1: no info ?", $pkg);
		return;
	}
	my $plist;
	if ($state->opt('I')) {
		if ($state->opt('q')) {
			$state->say("#1", $pkg);
		} else {
			$self->may_check_data($handle, $pkg, $state, \$plist);
			my $l = 20 - length($pkg);
			$l = 1 if $l <= 0;
			$state->say("#1#2#3", $pkg, " "x$l,
			    get_comment($handle->info));
		}
	} else {
		if ($state->opt('c')) {
			$self->may_check_data($handle, $pkg, $state, \$plist);
			$state->header($handle);
			$state->banner("Comment:");
			$state->say("#1\n", get_comment($handle->info));
		}
		if ($state->opt('R') && -f $handle->info.REQUIRED_BY) {
			$state->header($handle);
			$state->banner("Required by:");
			$state->printfile_sorted($handle->info.REQUIRED_BY);
		}
		if ($state->opt('d')) {
			$self->may_check_data($handle, $pkg, $state, \$plist);
			$state->header($handle);
			$state->banner("Description:");
			$state->print_description($handle->info);
		}
		if ($state->opt('M') && -f $handle->info.DISPLAY) {
			$self->may_check_data($handle, $pkg, $state, \$plist);
			$state->header($handle);
			$state->banner("Install notice:");
			$state->printfile($handle->info.DISPLAY);
		}
		if ($state->opt('U') && -f $handle->info.UNDISPLAY) {
			$self->may_check_data($handle, $pkg, $state, \$plist);
			$state->header($handle);
			$state->banner("Deinstall notice:");
			$state->printfile($handle->info.UNDISPLAY);
		}
		my $needplist = $state->hasanyopt('fsSC');
		if ($needplist || $state->opt('L')) {
			require OpenBSD::PackingList;

			$self->may_check_data($handle, $pkg, $state, \$plist);
			if ($needplist) {
				$plist //= $handle->plist;
			} else {
				$plist //= $handle->plist(\&OpenBSD::PackingList::FilesOnly);
			}
			$state->fatal("bad packing-list for #1", $handle->url)
			    unless defined $plist;
		}
		if ($state->opt('L')) {
			$state->header($handle);
			$state->banner("Files:");
			$plist->dump_file($state->opt('K'));
			$state->say;
		}
		if ($state->opt('C')) {
			$state->header($handle);
			if ($plist->is_signed) {
				my $sig = $plist->get('digital-signature');
				if ($sig->{key} eq 'x509') {
					require OpenBSD::x509;
					$state->banner("Certificate info:");
					OpenBSD::x509::print_certificate_info($plist);
				} elsif ($sig->{key} eq 'signify') {
					$state->say("reportedly signed by #1", 
					    $plist->get('signer')->name);
				}
			} else {
				$state->banner("No digital signature");
			}
		}
		if ($state->opt('s')) {
			$state->header($handle);
			my $size = 0;
			$plist->sum_up(\$size);
			$state->say(
			    ($state->opt('q') ? "#1": "Size: #1"), $size);
			$total_size += $size;
			$pkgs++;
		}
		if ($state->opt('S')) {
			$state->header($handle);
			$state->say(
			    ($state->opt('q') ? "#1": "Signature: #1"),
			    $plist->signature->string);
		}
		if ($state->opt('P')) {
			require OpenBSD::PackingList;

			my $plist = $handle->plist(
			    \&OpenBSD::PackingList::ExtraInfoOnly);
			$state->header($handle);
			$state->banner("Pkgpath:");
			if (defined $plist->fullpkgpath) {
				$state->say("#1", $plist->fullpkgpath);
			} else {
				$state->errsay("#1 has no FULLPKGPATH", $plist->pkgname);
				$state->say;
			}
		}

		if ($state->opt('f')) {
			$state->header($handle);
			$state->banner("Packing-list:");
			$plist->write(\*STDOUT);
			$state->say;
		}
		$state->footer($handle);
	}
}

sub parse_and_run
{
	my ($self, $cmd) = @_;
	my $exit_code = 0;
	my @sought_files;
	my $error_e = 0;
	my $state = OpenBSD::PkgInfo::State->new($cmd);
	my @extra;
	$state->{opt} =
	    {
	    	'e' =>
		    sub {
			    my $pat = shift;
			    my @list;
			    if ($pat =~ m/\//o) {
				    $state->lock;
				    @list = find_by_path($pat);
				    push(@ARGV, @list);
			    } else {
				    @list = find_by_spec($pat, $state);
				    push(@extra, @list);
			    }
			    if (@list == 0) {
				    $exit_code = 1;
				    $error_e = 1;
			    }
			    $state->{terse} = 1;
		    },
	     'E' =>
		    sub {
			    require File::Spec;

			    push(@sought_files, File::Spec->rel2abs(shift));

		    }
	    };
	$state->{no_exports} = 1;
	$state->handle_options('cCdfF:hIKLmPQ:qr:RsSUe:E:Ml:aAt',
	    '[-AaCcdfIKLMmPqRSstUv] [-D nolock][-E filename] [-e pkg-name] ',
	    '[-l str] [-Q query] [-r pkgspec] [pkg-name] [...]');

	if ($state->opt('r')) {

		require OpenBSD::PkgSpec;

		my $pattern = $state->opt('r');
		my $s = OpenBSD::PkgSpec->new($pattern);
		if (!$s->is_valid) {
			$state->errsay("Invalid pkgspec: #1", $pattern);
			return 1;
		}
		my @l = $s->match_ref(\@ARGV);
		unless ($state->opt('q')) {
			$state->say("Pkgspec #1 matched #2", $pattern,
			    join(' ', @l));
		}
		if (@l == 0) {
			$exit_code += 2;
		}
		if (@extra == 0) {
			return $exit_code;
		} else {
			@ARGV = ();
		}
	}

	$state->lock;

	my $nonames = @ARGV == 0 && @extra == 0;

	unless ($state->hasanyopt('cMUdfILRsSP') || $state->{terse}) {
		if ($nonames) {
			$state->setopts('Ia');
		} else {
			$state->setopts('cdMR');
		}
	}

	if ($state->opt('Q')) {
		require OpenBSD::Search;

		print "PKG_PATH=$ENV{PKG_PATH}\n" if $state->verbose;
		my $partial = OpenBSD::Search::PartialStem->new($state->opt('Q'));
		my $r = $state->repo->match_locations($partial);

		for my $p (sort map {$_->name} @$r) {
			$state->say(
			    is_installed($p) ? "#1 (installed)" : "#1", $p);
		}

		return 0;
	}

	if ($state->verbose) {
		$state->setopts('cdfMURsS');
	}

	if ($state->opt('K') && !$state->opt('L')) {
		$state->usage("-K only makes sense with -L");
	}

	my $all = $state->opt('a') || $state->opt('A');

	if ($nonames && !$all) {
		$state->usage("Missing package name(s)") unless $state->{terse} || $state->opt('q');
	}

	if (!$nonames && $state->hasanyopt('aAtm')) {
		$state->usage("Can't specify package name(s) with [-aAtm]");
	}


	if ($nonames && !$error_e) {
		@ARGV = sort(installed_packages($state->opt('A') ? 0 : 1));
		if ($state->opt('t')) {
			require OpenBSD::RequiredBy;
			@ARGV = grep { OpenBSD::RequiredBy->new($_)->list == 0 } @ARGV;
		}
	}

	if (@sought_files) {
		my %hash = map { ($_, []) }  @sought_files;
		@ARGV = $self->filter_files($state, \%hash, @ARGV);
		for my $f (@sought_files) {
			my $l = $hash{$f};
			if (@$l) {
				$state->say("#1: #2", $f, join(',', @$l))
				    unless $state->opt('q');
			} else {
				$exit_code = 1;
			}
		}
	}

	if ($state->opt('m')) {
		@ARGV = $self->manual_filter($state, @ARGV);
	}

	for my $pkg (@ARGV) {
		if ($state->{terse}) {
			$state->banner('#1', $pkg);
		}
		$self->find_pkg($state, $pkg,
		    sub {
			$self->print_info($state, @_);
		});
	}
	for my $extra (@extra) {
		if ($state->{terse}) {
			$state->banner('#1', $extra->url);
		}
		$self->print_info($state, $extra->url, $extra);
	}

	if ($pkgs > 1) {
		$state->say("Total size: #1", $total_size);
	}
	return $exit_code;
}

1;
