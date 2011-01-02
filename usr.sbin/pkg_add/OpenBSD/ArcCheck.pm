# ex:ts=8 sw=4:
# $OpenBSD: ArcCheck.pm,v 1.21 2011/01/02 15:25:45 espie Exp $
#
# Copyright (c) 2005-2006 Marc Espie <espie@openbsd.org>
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

# Supplementary code to handle archives in the package context.
# Contrarily to GNU-tar, we do not change the archive format, but by
# convention,  the names LongName\d+ and LongLink\d correspond to names
# too long to fit. The actual names reside in the PLIST, but the archive
# is still a valid archive.

use strict;
use warnings;

use OpenBSD::Ustar;

package OpenBSD::Ustar::Object;

# match archive header name against PackingElement item
sub check_name
{
	my ($self, $item) = @_;
	return 1 if $self->name eq $item->name;
	if ($self->name =~ m/^LongName\d+$/o) {
		$self->set_name($item->name);
		return 1;
	}
	return 0;
}

# match archive header link name against actual link names
sub check_linkname
{
	my ($self, $linkname) = @_;
	my $c = $self->{linkname};
	if ($self->isHardLink && defined $self->{cwd}) {
		$c = $self->{cwd}.'/'.$c;
	}
	return 1 if $c eq $linkname;
	if ($self->{linkname} =~ m/^Long(?:Link|Name)\d+$/o) {
		$self->{linkname} = $linkname;
		if ($self->isHardLink && defined $self->{cwd}) {
			$self->{linkname} =~ s|^$self->{cwd}/||;
		}
		return 1;
	}
	return 0;
}

use POSIX;

sub verify_modes
{
	my ($o, $item) = @_;
	my $result = 1;

	if (!defined $item->{owner} && !$o->isSymLink) {
	    if ($o->{uname} ne 'root' && $o->{uname} ne 'bin') {
		    $o->errsay("Error: no \@owner for #1 (#2)",
			$item->fullname, $o->{uname});
	    		$result = 0;
	    }
	}
	if (!defined $item->{group} && !$o->isSymLink) {
	    if ($o->{gname} ne 'bin' && $o->{gname} ne 'wheel') {
		if (($o->{mode} & (S_ISUID | S_ISGID | S_IWGRP)) != 0) {
		    $o->errsay("Error: no \@group for #1 (#2), which has mode #3",
			$item->fullname, $o->{uname},
			sprintf("%4o", $o->{mode} & (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID)));
	    		$result = 0;
		} else {
		    $o->errsay("Warning: no \@group for #1 (#2)",
			$item->fullname, $o->{gname});
	    	}
	    }
	}
	if (!defined $item->{mode} && $o->isFile) {
	    if (($o->{mode} & (S_ISUID | S_ISGID | S_IWOTH)) != 0) {
		    $o->errsay("Error: weird mode for #1: #2",
			$item->fullname,
			sprintf("%4o", $o->{mode} & (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID)));
	    		$result = 0;
	    }
	}
	return $result;
}

# copy long items, avoiding duplicate long names.
sub copy_long
{
	my ($self, $wrarc) = @_;
	if ($self->name =~ m/^LongName(\d+)$/o) {
		$wrarc->{name_index} = $1 + 1;
	}
	if (length($self->name) >
	    OpenBSD::Ustar::MAXFILENAME + OpenBSD::Ustar::MAXPREFIX + 1) {
		$wrarc->{name_index} = 0 if !defined $wrarc->{name_index};
		$self->set_name('LongName'.$wrarc->{name_index}++);
	}
	$self->copy($wrarc);
}

package OpenBSD::Ustar;

# prepare item and introduce long names where needed.
sub prepare_long
{
	my ($self, $item) = @_;
	my $entry;
	if (defined $item->{wtempname}) {
		$entry = $self->prepare($item->{wtempname}, '');
	} else {
		$entry = $self->prepare($item->name);
	}
	if (!defined $entry->{uname}) {
		$self->fatal("No user name for #1 (uid #2)",
		    $item->name, $entry->{uid});
	}
	if (!defined $entry->{gname}) {
		$self->fatal("No group name for #1 (uid #2)",
		    $item->name, $entry->{gid});
	}

	$entry->set_name($item->name);
	my ($prefix, $name) = split_name($entry->name);
	if (length($name) > MAXFILENAME || length($prefix) > MAXPREFIX) {
		$self->{name_index} = 0 if !defined $self->{name_index};
		$entry->set_name('LongName'.$self->{name_index}++);
	}
	if ((defined $entry->{linkname}) &&
	    length($entry->{linkname}) > MAXLINKNAME) {
		$self->{linkname_index} = 0 if !defined $self->{linkname_index};
		$entry->{linkname} = 'LongLink'.$self->{linkname_index}++;
	}
	return $entry;
}

1;
