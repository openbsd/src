# ex:ts=8 sw=4:
# $OpenBSD: Logger.pm,v 1.3 2004/08/06 07:51:17 espie Exp $
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
package OpenBSD::Logger;

use OpenBSD::Temp;
use File::Temp;

my $log_handle;
my $log_base;
my $log_name;
my @annotations=();

sub log_as($)
{
	$log_base = shift;
}

sub logname()
{
	return $log_name;
}

sub logfile()
{
	if (!defined $log_handle) {
		($log_handle, $log_name) =  
		    File::Temp::tempfile("$log_base.XXXXXXXXXXX", DIR => $OpenBSD::Temp::tempbase);
	}
	return $log_handle;
}

sub annotate
{
	push(@annotations, @_);
}

sub log(@)
{
	my $fh = logfile();
	if (@annotations > 0) {
		print $fh @annotations;
		@annotations = ();
	}
	print $fh @_;
}

1;
