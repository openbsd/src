# $OpenBSD: LaLoFile.pm,v 1.1 2012/06/19 09:30:44 espie Exp $

# Copyright (c) 2007-2010 Steven Mestdagh <steven@openbsd.org>
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
use feature qw(say switch state);

package LT::LaLoFile;
my %file_cache;		# which files have been parsed
my $cache_by_fullname = {};
my $cache_by_inode = {};

# allows special treatment for some keywords
sub set
{
	my ($self, $k, $v) = @_;

	$self->{$k} = $v;
}

sub stringize
{
	my ($self, $k) = @_;
	if (defined $self->{$k}) {
	       return $self->{$k};
	}
	return '';
}

sub read
{
	my ($class, $filename) = @_;
	my $info = $class->new;
	open(my $fh, '<', $filename) or die "Cannot read $filename: $!\n";
	my $_;
	while (<$fh>) {
		chomp;
		next if /^\#/;
		next if /^\s*$/;
		if (m/^(\S+)\=\'(.*)\'$/) {
			$info->set($1, $2);
		} elsif (m/^(\S+)\=(\S+)$/) {
			$info->set($1, $2);
		}
	}
	return $info;
}

sub parse
{
	my ($class, $filename) = @_;

	LT::Trace::debug {"parsing $filename"};

	if (defined $cache_by_fullname->{$filename}) {
		LT::Trace::debug {" (cached)\n"};
		return $cache_by_fullname->{$filename};
	}
	my $key = join("/", (stat $filename)[0,1]);
	if (defined $cache_by_inode->{$key}) {
		LT::Trace::debug {" (cached)\n"};
		return $cache_by_inode->{$key};
	}
	LT::Trace::debug {"\n"};
	return $cache_by_inode->{$key} = $cache_by_fullname->{$filename} =
	    $class->read($filename);
}

sub new
{
	my $class = shift;
	bless {}, $class;
}

1;
