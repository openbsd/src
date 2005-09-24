# ex:ts=8 sw=4:
# $OpenBSD: ArcCheck.pm,v 1.3 2005/09/24 12:52:19 espie Exp $
#
# Copyright (c) 2005 Marc Espie <espie@openbsd.org>
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

# Supplementary code to handle archives.

package OpenBSD::Ustar::Object;

sub check_name
{
	my ($self, $item) = @_;
	return 1 if $self->{name} eq $item->{name};
	if ($self->{name} =~ m/^LongName\d+$/) {
		$self->{name} = $item->{name};
		return 1;
	}
	return 0;
}

sub check_linkname
{
	my ($self, $linkname) = @_;
	my $c = $self->{linkname};
	if ($self->isHardLink() && defined $self->{cwd}) {
		$c = $self->{cwd}.'/'.$c;
	}
	return 1 if $c eq $linkname;
	if ($self->{linkname} =~ m/^Long(?:Link|Name)\d+$/) {
		$self->{linkname} = $linkname;
		if ($self->isHardLink() && defined $self->{cwd}) {
			$self->{linkname} =~ s|^$self->{cwd}/||;
		}
		return 1;
	}
	return 0;
}

sub copy_long
{
	my ($self, $wrarc) = @_;
	if ($self->{name} =~ m/^LongName(\d+)$/) {
		$wrarc->{name_index} = $1 + 1;
	}
	if (length($self->{name}) > MAXFILENAME+MAXPREFIX+1) {
		$wrarc->{name_index} = 0 if !defined $wrarc->{name_index};
		$entry->{name} = 'LongName'.$wrarc->{name_index}++;
	}
	$self->copy($wrarc);
}

package OpenBSD::Ustar;

sub prepare_long
{
	my ($self, $item) = @_;
	my $filename = $item->{name};
	my $entry = $self->prepare($filename);
	if (!defined $entry->{uname}) {
		die "No user name for ", $entry->{name}, " (uid ", $entry->{uid}, ")\n";
	}
	if (!defined $entry->{gname}) {
		die "No group name for ", $entry->{name}, " (gid ", $entry->{gid}. "\n";
	}
	my ($prefix, $name) = split_name($entry->{name});
	if (length($name) > MAXFILENAME || length($prefix) > MAXPREFIX) {
		$self->{name_index} = 0 if !defined $self->{name_index};
		$entry->{name} = 'LongName'.$self->{name_index}++;
	}
	if (length($entry->{linkname}) > MAXLINKNAME) {
		$self->{linkname_index} = 0 if !defined $self->{linkname_index};
		$entry->{linkname} = 'LongLink'.$self->{linkname_index}++;
	}
	return $entry;
}

1;
