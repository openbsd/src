# ex:ts=8 sw=4:
# $OpenBSD: ArcCheck.pm,v 1.30 2014/11/30 16:44:04 espie Exp $
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
	return $self->name eq $item->name;
}

# match archive header link name against actual link names
sub check_linkname
{
	my ($self, $linkname) = @_;
	my $c = $self->{linkname};
	if ($self->isHardLink && defined $self->{cwd}) {
		$c = $self->{cwd}.'/'.$c;
	}
	return $c eq $linkname;
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
	    if (($o->{mode} & (S_ISUID | S_ISGID | S_IWOTH)) != 0 ||
	    	($o->{mode} & S_IROTH) == 0 || ($o->{mode} & S_IRGRP) == 0) {
		    $o->errsay("Error: weird mode for #1: #2",
			$item->fullname,
			sprintf("%4o", $o->{mode} & (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID)));
	    		$result = 0;
	    }
	}
	if ($o->isFile) {
		if (!defined $item->{size}) {
			$o->errsay("Error: file #1 does not have recorded size",
			    $item->fullname);
			$result = 0;
		} elsif ($item->{size} != $o->{size}) {
			$o->errsay("Error: size does not match for #1",
			    $item->fullname);
			$result = 0;
		}
	}
	return $result;
}

package OpenBSD::Ustar;
use POSIX;

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
	if (defined $item->{owner}) {
		$entry->{uname} = $item->{owner};
		if (defined $item->{uid}) {
			$entry->{uid} = $item->{uid};
		} else {
			delete $entry->{uid};
		}
	} else {
		$entry->{uname} = "root";
		delete $entry->{uid};
	}
	if (defined $item->{group}) {
		$entry->{gname} = $item->{group};
		if (defined $item->{gid}) {
			$entry->{gid} = $item->{gid};
		} else {
			delete $entry->{gid};
		}
	} else {
		$entry->{gname} = "bin";
		delete $entry->{gid};
	}
	# likewise, we skip links on extractions, so hey, don't even care
	# about modes and stuff.
	if ($entry->isSymLink) {
		$entry->{mode} = 0777;
		$entry->{uname} = 'root';
		$entry->{gname} = 'wheel';
		delete $entry->{uid};
		delete $entry->{gid};
	}
	$entry->recheck_owner;
	if (!defined $entry->{uname}) {
		$self->fatal("No user name for #1 (uid #2)",
		    $item->name, $entry->{uid});
	}
	if (!defined $entry->{gname}) {
		$self->fatal("No group name for #1 (gid #2)",
		    $item->name, $entry->{gid});
	}
	# disallow writable files/dirs without explicit annotation
	if (!defined $item->{mode}) {
		$entry->{mode} &= ~(S_IWUSR|S_IWGRP|S_IWOTH);
	}
	# if we're going to set the group or owner, sguid bits won't
	# survive the extraction
	if (defined $item->{group} || defined $item->{owner}) {
		$entry->{mode} &= ~(S_ISUID|S_ISGID);
	}
	if (defined $item->{ts}) {
		delete $entry->{mtime};
	}

	$entry->set_name($item->name);
	return $entry;
}

1;
