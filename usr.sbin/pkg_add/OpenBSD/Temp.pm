# ex:ts=8 sw=4:
# $OpenBSD: Temp.pm,v 1.29 2018/10/04 09:17:00 espie Exp $
#
# Copyright (c) 2003-2005 Marc Espie <espie@openbsd.org>
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

package OpenBSD::Temp;

use OpenBSD::MkTemp;
use OpenBSD::Paths;
use OpenBSD::Error;

our $tempbase = $ENV{'PKG_TMPDIR'} || OpenBSD::Paths->vartmp;

my $dirs = {};
my $files = {};

my $cleanup = sub {
	while (my ($name, $pid) = each %$files) {
		unlink($name) if $pid == $$;
	}
	while (my ($dir, $pid) = each %$dirs) {
		OpenBSD::Error->rmtree([$dir]) if $pid == $$;
	}
};

END {
	&$cleanup;
}
OpenBSD::Handler->register($cleanup);

sub dir
{
	my $caught;
	my $h = sub { $caught = shift; };
	my $dir;

	{
	    local $SIG{'INT'} = $h;
	    local $SIG{'QUIT'} = $h;
	    local $SIG{'HUP'} = $h;
	    local $SIG{'KILL'} = $h;
	    local $SIG{'TERM'} = $h;
	    $dir = permanent_dir($tempbase, "pkginfo");
	    $dirs->{$dir} = $$;
	}
	if (defined $caught) {
		kill $caught, $$;
	}
	return "$dir/";
}

sub fh_file
{
	my ($stem, $cleanup) = @_;
	my $caught;
	my $h = sub { $caught = shift; };
	my ($fh, $file);

	{
	    local $SIG{'INT'} = $h;
	    local $SIG{'QUIT'} = $h;
	    local $SIG{'HUP'} = $h;
	    local $SIG{'KILL'} = $h;
	    local $SIG{'TERM'} = $h;
	    ($fh, $file) = permanent_file($tempbase, $stem);
	    if (defined $file) {
		    &$cleanup($file);
	    }
	}
	if (defined $caught) {
		kill $caught, $$;
	}
	return ($fh, $file);
}

sub file
{
	return (fh_file("pkgout", 
	    sub { my $n = shift; $files->{$n} = $$; })) [1];
}

sub reclaim
{
	my ($class, $name) = @_;
	delete $files->{$name};
	delete $dirs->{$name};
}

sub permanent_file
{
	my ($dir, $stem) = @_;
	my $template = "$stem.XXXXXXXXXX";
	if (defined $dir) {
		$template = "$dir/$template";
	}
	return OpenBSD::MkTemp::mkstemp($template);
}

sub permanent_dir
{
	my ($dir, $stem) = @_;
	my $template = "$stem.XXXXXXXXXX";
	if (defined $dir) {
		$template = "$dir/$template";
	}
	return OpenBSD::MkTemp::mkdtemp($template);
}

1;
