# ex:ts=8 sw=4:
# $OpenBSD: Paths.pm,v 1.1 2007/06/10 16:59:30 espie Exp $
#
# Copyright (c) 2007 Marc Espie <espie@openbsd.org>
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

package OpenBSD::Paths;

# Commands
our $ldconfig = '/sbin/ldconfig';
our $mkfontdir = '/usr/X11R6/bin/mkfontdir';
our $fc_cache = '/usr/X11R6/bin/fc-cache';
our $install_info = '/usr/bin/install-info';
our $useradd = '/usr/sbin/useradd';
our $groupadd = '/usr/sbin/groupadd';
our $sysctl = '/sbin/sysctl';
our $openssl = '/usr/sbin/openssl';
our $chmod = '/bin/chmod';
our $gzip = '/usr/bin/gzip';
our $ftp = '/usr/bin/ftp';
our $groff = '/usr/bin/groff';
our $sh = '/bin/sh';
our $arch = '/usr/bin/arch';
our $uname = '/usr/bin/uname';
our $userdel = '/usr/sbin/userdel';
our $groupdel = '/usr/sbin/groupdel';
our $mknod = '/sbin/mknod';
our $mount = '/sbin/mount';
our $df = '/bin/df';
our $ssh = '/us/bin/ssh';
our $make = '/usr/bin/make';
our $mklocatedb = '/usr/libexec/locate.mklocatedb';

# Various paths
our $shells = '/etc/shells';
our $pkgdb = '/var/db/pkg';
our $localbase = '/usr/local';
our $vartmp = '/var/tmp';
our $portsdir = '/usr/ports';

our @library_dirs = ("/usr", "/usr/X11R6");
our @master_keys = ("/etc/master_key");

our @font_cruft = ("fonts.alias", "fonts.dir", "fonts.cache-1");
our @man_cruft = ("whatis.db");

1;
