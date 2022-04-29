# ex:ts=8 sw=4:
# $OpenBSD: Cache.pm,v 1.5 2022/04/29 10:44:05 espie Exp $
#
# Copyright (c) 2022 Marc Espie <espie@openbsd.org>
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

# supplementary glue to add support for reading the update.db locate(1)
# database in quirks
package OpenBSD::PackageRepository::Cache;

sub new
{
	my ($class, $state, $setlist) = @_;

	return undef unless -f OpenBSD::Paths->updateinfodb;

	my $o = bless { 
	    raw_data => {}, 
	    stems => {},
	    state => $state }, $class;

	$o->prime_update_info_cache($state, $setlist);
	return $o;

}
sub pipe_locate
{
	my ($self, @params) = @_;
	unshift(@params, OpenBSD::Paths->locate, 
	    '-d', OpenBSD::Paths->updateinfodb, '--');
	my $state = $self->{state};
	$state->errsay("Running #1", join(' ', @params))
	    if $state->defines("CACHING_VERBOSE");
	return @params;
}

sub prime_update_info_cache
{
	my ($self, $state, $setlist) = @_;

	my $progress = $state->progress;
	my $uncached = {};
	my $found = {};
	# figure out a list of names to precache

	# okay, so basically instead of hitting locate once for each
	# package on the distant repository, we precache all the stems
	# we are asking to update/install
	# this is based on the assumption that most names are "regular"
	# and we won't cache too little or too much
	for my $set (@{$setlist}) {
		for my $h ($set->older, $set->hints) {
			next if $h->{update_found};
			my $name = $h->pkgname;
			my $stem = OpenBSD::PackageName::splitstem($name);
			next if $stem =~ m/^\.libs\d*\-/;
			next if $stem =~ m/^partial\-/;
			$stem =~ s/\%.*//; # zap branch info
			$stem =~ s/\-\-.*//; # and set flavors
			$self->{stems}{$stem} = 1;
		}
	}
	# TODO actually ask quirks to extend the stemlist !
	my @list = sort keys %{$self->{stems}};
	return if @list == 0;
	$progress->set_header(
	    $state->f("Precaching update information for #1 names...", 
		scalar(@list)));
	open my $fh, "-|", $self->pipe_locate(map { "$_-[0-9]*"} @list) or die $!;
	while (<$fh>) {
		$progress->working(100);
		if (m/^(.*?)\:(.*)/) {
			my ($pkgname, $value) = ($1, $2);
			$found->{OpenBSD::PackageName::splitstem($pkgname)} = 1;
			$self->{raw_data}{$pkgname} //= '';
			$self->{raw_data}{$pkgname} .= "$value\n";
			if ($value =~ m/\@option\s+always-update/) {
				$uncached->{$pkgname} = 1;
			}
		}
	}
	close($fh);
	for my $pkgname (keys %$uncached) {
		delete $self->{raw_data}{$pkgname}
	}
	return unless $state->defines("CACHING_VERBOSE");
	for my $k (@list) {
		if (!defined $found->{$k}) {
			$state->say("No cache entry for #1", $k);
		}
	}
}

sub get_cached_info
{
	my ($self, $name) = @_;

	my $state = $self->{state};
	my $content;
	if (exists $self->{raw_data}{$name}) {
		$content = $self->{raw_data}{$name};
	} else {
		my $stem = OpenBSD::PackageName::splitstem($name);
		if (exists $self->{stems}{$stem}) {
			$state->say("Negative caching for #1", $name)
			    if $state->defines("CACHING_VERBOSE");
			return undef;
		}
		$content = '';
		open my $fh, "-|", $self->pipe_locate($name.":*") or die $!;
		while (<$fh>) {
			if (m/\@option\s+always-update/) {
				return undef;
			}
			if (m/^.*?\:(.*)/) {
				$content .= $1."\n";
			} else {
				return undef;
			}
		}
		close ($fh);
	}
	if ($content eq '') {
		$state->say("Cache miss for #1", $name)
		    if $state->defines("CACHING_VERBOSE");
		return undef;
	}
	open my $fh2, "<", \$content;
	return OpenBSD::PackingList->read($fh2);
}

1;
